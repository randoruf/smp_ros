/*! \file planners/rrt.h
  \brief An implementation of a Rapidly-exploring Random Tree (RRT) algorithm.

  Provides an implementation of the RRT algorithm. Inherits from the generic
  incremental sampling-based motion planner, overriding the iteration function.

  * Copyright (C) 2018 Sertac Karaman
  *
  * This program is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with this program.  If not, see <http://www.gnu.org/licenses/>
  *
  */

#ifndef _SMP_PLANNER_RRT_H_
#define _SMP_PLANNER_RRT_H_

#include <smp/planners/base_incremental.h>

namespace smp {

//! RRT algorithm.
/*!
  Provides an implementation of the RRT algorithm. Inherits from the generic
  incremental sampling-based motion planner, overriding the iteration function.

  \ingroup planners
*/
template <class typeparams> class rrt : public planner_incremental<typeparams> {

  typedef typename typeparams::state state_t;
  typedef typename typeparams::input input_t;

  typedef trajectory<typeparams> trajectory_t;

  typedef vertex<typeparams> vertex_t;
  typedef edge<typeparams> edge_t;

  typedef planner_incremental<typeparams> planner_t;

  typedef sampler_base<typeparams> sampler_t;
  typedef distance_evaluator_base<typeparams> distance_evaluator_t;
  typedef extender_base<typeparams> extender_t;
  typedef collision_checker_base<typeparams> collision_checker_t;
  typedef model_checker_base<typeparams> model_checker_t;

private:
  // This function adds the given state to the beginning of the tracjetory and
  // calls the collision checker.
  int check_extended_trajectory_for_collision(state_t *state,
                                              trajectory_t *trajectory) {

    trajectory->list_states.push_front(state);
    int collision_check =
        this->collision_checker.check_collision_trajectory(trajectory);
    trajectory->list_states.pop_front();

    return collision_check;
  }

public:
  rrt();
  ~rrt();

  /**
   * \brief A constructor that initializes all components.
   *
   * This is the recommended constructor that initializes all components all at
   * once.
   * It calls the corresponding constructor of the base class
   * planner_incremental<typeparams> with the same arguments.
   *
   * @param sampler_in New sampler component.
   * @param distance_evaluator_in New distance evaluator component.
   * @param extender_in New extension function component.
   * @param collision_checker_in New collision checker component.
   * @param model_checker_in New model checker component.
   */
  rrt(sampler_t &sampler_in, distance_evaluator_t &distance_evaluator_in,
      extender_t &extender_in, collision_checker_t &collision_checker_in,
      model_checker_t &model_checker_in);

  /**
   * \brief A function call to initiate one iteration of the algorithm.
   *
   * Runs one iteration of the RRT algorithm which includes the following steps:
   * - get one sample state (using the sampler component)
   * - find the vertex in the graph that is nearest to the sample state
   * (using the distance evaluator component)
   * - generate a trajectory that starts from the state stored in the nearest
   * vertex and reaches exactly or approximately to the sample state (using
   * the extension function component)
   * - check whether the new trajectory satsifies the conditions for being
   * collision free (using the collision checker component).
   * - if the new trajectory is collision free, then add it into the graph
   * and incrementally check whether the graph includes a trajectory that
   * satisfies the termination requirement (using the model checker component).
   *
   * @returns Returns 1 for success, and a non-positive number for failure.
   */
  int iteration();
};
}

#endif
