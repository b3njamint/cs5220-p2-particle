#include <string.h>
#include <vector>
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

void hash_particles(sim_state_t* s, float h, std::vector<omp_lock_t>& bin_locks)
{
    /* BEGIN TASK */
    // each bucket contains a linked list of particles
    memset(s->hash, 0, sizeof(particle_t*) * HASH_SIZE);

    // for each particle, calculate bucket and add to it
    for (int i = 0; i < s->n; i++) {
        unsigned bucket = particle_bucket(&s->part[i], h);
        omp_set_lock(&bin_locks[bucket]);
        s->part[i].next = s->hash[bucket];
        s->hash[bucket] = &s->part[i];
        omp_set_lock(&bin_locks[bucket]);
    } 

    /* END TASK */
}
