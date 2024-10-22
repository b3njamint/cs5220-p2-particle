#include <string.h>
#include <omp.h>

#include "zmorton.hpp"
#include "binhash.hpp"

/*@q
 * ====================================================================
 */

/*@T
 * \subsection{Spatial hashing implementation}
 * 
 * In the current implementation, we assume [[HASH_DIM]] is $2^b$,
 * so that computing a bitwise of an integer with [[HASH_DIM]] extracts
 * the $b$ lowest-order bits.  We could make [[HASH_DIM]] be something
 * other than a power of two, but we would then need to compute an integer
 * modulus or something of that sort.
 * 
 *@c*/

#define HASH_MASK (HASH_DIM-1)
#define USE_PARALLEL

unsigned particle_bucket(particle_t* p, float h)
{
    unsigned ix = p->x[0]/h;
    unsigned iy = p->x[1]/h;
    unsigned iz = p->x[2]/h;
    return zm_encode(ix & HASH_MASK, iy & HASH_MASK, iz & HASH_MASK);
}

unsigned particle_neighborhood(unsigned* buckets, particle_t* p, float h)
{
    /* BEGIN TASK */    

    unsigned ix = p->x[0] / h;
    unsigned iy = p->x[1] / h;
    unsigned iz = p->x[2] / h;
    int count = 0;

    // iterate through all neighbors
    #ifdef USE_PARALLEL
    #pragma omp parallel for
    #endif
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dz = -1; dz <= 1; dz++) {
                unsigned neighbor_x = (ix + dx) & HASH_MASK;
                unsigned neighbor_y = (iy + dy) & HASH_MASK;
                unsigned neighbor_z = (iz + dz) & HASH_MASK;

                unsigned neighbor_bucket = zm_encode(neighbor_x, neighbor_y, neighbor_z);

                if (count < MAX_NBR_BINS) {
                    buckets[count++] = neighbor_bucket;
                }
            }
        }
    } 

    return count;

    /* END TASK */
}

void hash_particles(sim_state_t* s, float h)
{
    /* BEGIN TASK */
    particle_t** hash = s->hash;

    #ifdef USE_PARALLEL
    omp_lock_t hash_locks[HASH_SIZE];
    for (int i = 0; i < HASH_SIZE; ++i) {
        omp_init_lock(&hash_locks[i]);
    }
    #endif

    // each bucket contains a linked list of particles
    memset(hash, 0, sizeof(particle_t*) * HASH_SIZE);

    // for each particle, calculate bucket and add to it
    #ifdef USE_PARALLEL
    #pragma omp parallel for
    #endif
    for (int i = 0; i < s->n; i++) {
        particle_t* pi = &s->part[i];
        unsigned bucket = particle_bucket(pi, h);

        #ifdef USE_PARALLEL
        omp_set_lock(&hash_locks[bucket]);
        #endif
        {
            pi->next = hash[bucket];
            hash[bucket] = pi;
        }
        #ifdef USE_PARALLEL
        omp_unset_lock(&hash_locks[bucket]);
        #endif
    }

    #ifdef USE_PARALLEL
    for (int i = 0; i < HASH_SIZE; ++i) {
        omp_destroy_lock(&hash_locks[i]);
    }
    #endif

    /* END TASK */
}
