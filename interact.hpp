#ifndef INTERACT_HPP
#define INTERACT_HPP

#include "params.hpp"
#include "state.hpp"

#include <omp.h>
#include <vector>

void compute_density(sim_state_t* s, sim_param_t* params);
void compute_accel(sim_state_t* state, sim_param_t* params, std::vector<omp_lock_t>& bin_locks);

#endif /* INTERACT_HPP */
