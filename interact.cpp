#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "vec3.hpp"
#include "zmorton.hpp"

#include "params.hpp"
#include "state.hpp"
#include "interact.hpp"
#include "binhash.hpp"

/* Define this to use the bucketing version of the code */
#define USE_BUCKETING
/* Define this to use the parallel version of the code */
#define USE_PARALLEL

/*@T
 * \subsection{Density computations}
 * 
 * The formula for density is
 * \[
 *   \rho_i = \sum_j m_j W_{p6}(r_i-r_j,h)
 *          = \frac{315 m}{64 \pi h^9} \sum_{j \in N_i} (h^2 - r^2)^3.
 * \]
 * We search for neighbors of node $i$ by checking every particle,
 * which is not very efficient.  We do at least take advange of
 * the symmetry of the update ($i$ contributes to $j$ in the same
 * way that $j$ contributes to $i$).
 *@c*/

inline
void update_density(particle_t* pi, particle_t* pj, float h2, float C)
{
    float r2 = vec3_dist2(pi->x, pj->x);
    float z  = h2-r2;
    if (z > 0) {
        float rho_ij = C*z*z*z;

        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pi->rho += rho_ij;

        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pj->rho += rho_ij;
    }
}

void compute_density(sim_state_t* s, sim_param_t* params)
{
    int n = s->n;
    particle_t* p = s->part;
    particle_t** hash = s->hash;

    float h  = params->h;
    float h2 = h*h;
    float h3 = h2*h;
    float h9 = h3*h3*h3;
    float C  = ( 315.0/64.0/M_PI ) * s->mass / h9;

    // Clear densities
    #ifdef USE_PARALLEL
    #pragma omp parallel for
    #endif
    for (int i = 0; i < n; ++i)
        p[i].rho = 0;

    // Accumulate density info
    #ifdef USE_PARALLEL
    #pragma omp parallel for
    #endif
    /* BEGIN TASK */
    for (int i = 0; i < n; ++i) {
        #ifdef USE_BUCKETING
        particle_t* pi = &p[i];
        unsigned buckets[MAX_NBR_BINS];
        unsigned nbuckets = particle_neighborhood(buckets, pi, h);
        #else
        particle_t* pi = s->part+i;
        #endif
        
        // self contribution
        pi->rho += ( 315.0/64.0/M_PI ) * s->mass / h3;
        
        // neighbor contribution
        // #ifdef USE_PARALLEL
        // #pragma omp parallel for
        // #endif
        #ifdef USE_BUCKETING
        for (unsigned b = 0; b < nbuckets; ++b) {
            particle_t* pj = hash[buckets[b]];
            while (pj) {
                if (pj != pi) {
                    update_density(pi, pj, h2, C);
                }
                pj = pj->next;
            }
        }
        #else
        for (int j = i+1; j < n; ++j) {
            particle_t* pj = s->part+j;
            update_density(pi, pj, h2, C);
        }
        #endif
    }
    /* END TASK */
}


/*@T
 * \subsection{Computing forces}
 * 
 * The acceleration is computed by the rule
 * \[
 *   \bfa_i = \frac{1}{\rho_i} \sum_{j \in N_i} 
 *     \bff_{ij}^{\mathrm{interact}} + \bfg,
 * \]
 * where the pair interaction formula is as previously described.
 * Like [[compute_density]], the [[compute_accel]] routine takes
 * advantage of the symmetry of the interaction forces
 * ($\bff_{ij}^{\mathrm{interact}} = -\bff_{ji}^{\mathrm{interact}}$)
 * but it does a very expensive brute force search for neighbors.
 *@c*/

inline
void update_forces(particle_t* pi, particle_t* pj, float h2,
                   float rho0, float C0, float Cp, float Cv)
{
    float dx[3];
    vec3_diff(dx, pi->x, pj->x);
    float r2 = vec3_len2(dx);
    if (r2 < h2) {
        const float rhoi = pi->rho;
        const float rhoj = pj->rho;
        float q = sqrt(r2/h2);
        float u = 1-q;
        float w0 = C0 * u/rhoi/rhoj;
        float wp = w0 * Cp * (rhoi+rhoj-2*rho0) * u/q;
        float wv = w0 * Cv;
        float dv[3];
        vec3_diff(dv, pi->v, pj->v);

        // Equal and opposite pressure forces
        // vec3_saxpy(pi->a,  wp, dx);
        // vec3_saxpy(pj->a, -wp, dx);
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pi->a[0] += wp * dx[0];
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pi->a[1] += wp * dx[1];
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pi->a[2] += wp * dx[2];

        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pj->a[0] -= wp * dx[0];
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pj->a[1] -= wp * dx[1];
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pj->a[2] -= wp * dx[2];
        
        // Equal and opposite viscosity forces
        // vec3_saxpy(pi->a,  wv, dv);
        // vec3_saxpy(pj->a, -wv, dv);
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pi->a[0] += wv * dv[0];
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pi->a[1] += wv * dv[1];
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pi->a[2] += wv * dv[2];

        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pj->a[0] -= wv * dv[0];
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pj->a[1] -= wv * dv[1];
        #ifdef USE_PARALLEL
        #pragma omp atomic
        #endif
        pj->a[2] -= wv * dv[2];
    }
}

void compute_accel(sim_state_t* state, sim_param_t* params)
{
    // Unpack basic parameters
    const float h    = params->h;
    const float rho0 = params->rho0;
    const float k    = params->k;
    const float mu   = params->mu;
    const float g    = params->g;
    const float mass = state->mass;
    const float h2   = h*h;

    // Unpack system state
    particle_t* p = state->part;
    particle_t** hash = state->hash;
    int n = state->n;

    // Rehash the particles
    hash_particles(state, h);

    // Compute density and color
    compute_density(state, params);

    // Start with gravity and surface forces
    #ifdef USE_PARALLEL
    #pragma omp parallel for
    #endif
    for (int i = 0; i < n; ++i)
        vec3_set(p[i].a,  0, -g, 0);

    // Constants for interaction term
    float C0 = 45 * mass / M_PI / ( (h2)*(h2)*h );
    float Cp = k/2;
    float Cv = -mu;

    // Accumulate forces
    #ifdef USE_PARALLEL
    #pragma omp parallel for
    #endif
    /* BEGIN TASK */
    for (int i = 0; i < n; ++i) {
        #ifdef USE_BUCKETING
        particle_t* pi = &p[i];
        unsigned buckets[MAX_NBR_BINS];
        unsigned nbuckets = particle_neighborhood(buckets, pi, h);
        #else
        particle_t* pi = p+i;
        #endif
        
        // #ifdef USE_PARALLEL
        // #pragma omp parallel for
        // #endif
        #ifdef USE_BUCKETING
        for (unsigned b = 0; b < nbuckets; ++b) {
            particle_t* pj = hash[buckets[b]];
            while (pj) {
                if (pj > pi) {  // process each pair only once
                    update_forces(pi, pj, h2, rho0, C0, Cp, Cv);
                }
                pj = pj->next;
            }
        }
        #else
        for (int j = i+1; j < n; ++j) {
            particle_t* pj = p+j;
            update_forces(pi, pj, h2, rho0, C0, Cp, Cv);
        }
        #endif
    }
    /* END TASK */
}

