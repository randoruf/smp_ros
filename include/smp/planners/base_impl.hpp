/*
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

#ifndef _SMP_PLANNER_BASE_HPP_
#define _SMP_PLANNER_BASE_HPP_

template <class State, class Input> smp::planners::Base<State, Input>::Base() {
  list_vertices.clear();
  num_vertices = 0;
}

template <class State, class Input>
smp::planners::Base<State, Input>::Base(
    sampler_t &sampler_in, distance_evaluator_t &distance_evaluator_in,
    extender_t &extender_in, collision_checker_t &collision_checker_in,
    model_checker_t &model_checker_in)
    : sampler(sampler_in), distance_evaluator(distance_evaluator_in),
      extender(extender_in), collision_checker(collision_checker_in),
      model_checker(model_checker_in) {

  list_vertices.clear();
  num_vertices = 0;
}

template <class State, class Input> smp::planners::Base<State, Input>::~Base() {

  initialize(); // run the initialization function that clears up
                //     the memory occupied by the graph.
}

template <class State, class Input>
int smp::planners::Base<State, Input>::initialize() {

  // Delete all edges and vertices
  for (typename std::list<vertex_t *>::iterator iter_vertex =
           list_vertices.begin();
       iter_vertex != list_vertices.end(); iter_vertex++) {
    vertex_t *vertex_curr = *iter_vertex;
    for (typename std::list<edge_t *>::iterator iter_edge =
             vertex_curr->outgoing_edges.begin();
         iter_edge != vertex_curr->outgoing_edges.end(); iter_edge++) {
      edge_t *edge_curr = *iter_edge;
      delete edge_curr;
    }
  }

  for (typename std::list<vertex_t *>::iterator iter = list_vertices.begin();
       iter != list_vertices.end(); iter++) {
    vertex_t *vertex_curr = *iter;
    if (vertex_curr)
      delete vertex_curr;
  }

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::insert_vertex(vertex_t *vertex_in) {

  // insert the vertex to the list of vertices
  list_vertices.push_back(vertex_in);
  num_vertices++;

#if _SMP_FAST_VERTEX_DELETE

  vertex_in->it_vertex_list = list_vertices.end();
  (vertex_in->it_vertex_list)--;

#endif

  // UPDATE ALL COMPONENTS
  distance_evaluator.de_update_insert_vertex(vertex_in);
  model_checker.mc_update_insert_vertex(vertex_in);

  // Run all the update functions
  for (typename std::list<vertex_update_func_t>::iterator it_func =
           list_update_insert_vertex_functions.begin();
       it_func != list_update_insert_vertex_functions.end(); it_func++) {

    (*it_func)(vertex_in);
  }

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::delete_vertex(vertex_t *vertex_in) {

  // UPDATE ALL COMPONENTS
  distance_evaluator.de_update_delete_vertex(vertex_in);
  model_checker.mc_update_delete_vertex(vertex_in);

  // Run all the update functions
  for (typename std::list<vertex_update_func_t>::iterator it_func =
           list_update_delete_vertex_functions.begin();
       it_func != list_update_delete_vertex_functions.end(); it_func++) {
    (*it_func)(vertex_in);
  }

  std::list<edge_t *> edge_list;

  edge_list = vertex_in->incoming_edges;
  for (typename std::list<edge_t *>::iterator it_edge = edge_list.begin();
       it_edge != edge_list.end(); it_edge++) {
    edge_t *edge_curr = *it_edge;
    this->delete_edge(edge_curr);
  }
  edge_list.clear();

  edge_list = vertex_in->outgoing_edges;
  for (typename std::list<edge_t *>::iterator it_edge = edge_list.begin();
       it_edge != edge_list.end(); it_edge++) {
    edge_t *edge_curr = *it_edge;
    this->delete_edge(edge_curr);
  }
  edge_list.clear();

#if _SMP_FAST_VERTEX_DELETE

  typename std::list<vertex_t *>::iterator it_vertex_begin =
      vertex_in->it_vertex_list;
  typename std::list<vertex_t *>::iterator it_vertex_end = it_vertex_begin;
  it_vertex_end++;
  list_vertices.erase(it_vertex_begin, it_vertex_end);

#else

  list_vertices.remove(vertex_in);

#endif

  num_vertices--;

  delete vertex_in;

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::insert_edge(vertex_t *vertex_src_in,
                                                   edge_t *edge_in,
                                                   vertex_t *vertex_dst_in) {

  // WARNING: Overriding pointed data. May cause memory leaks.
  edge_in->vertex_src = vertex_src_in;
  edge_in->vertex_dst = vertex_dst_in;

  vertex_src_in->outgoing_edges.push_back(edge_in);
  vertex_dst_in->incoming_edges.push_back(edge_in);

  // UPDATE ALL COMPONENTS
  distance_evaluator.de_update_insert_edge(edge_in);
  model_checker.mc_update_insert_edge(edge_in);

  // Run all the update functions
  for (typename std::list<edge_update_func_t>::iterator it_func =
           list_update_insert_edge_functions.begin();
       it_func != list_update_insert_edge_functions.end(); it_func++) {
    (*it_func)(edge_in);
  }

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::delete_edge(edge_t *edge_in) {

  // UPDATE ALL COMPONENTS
  distance_evaluator.de_update_delete_edge(edge_in);
  model_checker.mc_update_delete_edge(edge_in);

  // Run all the update functions
  for (typename std::list<edge_update_func_t>::iterator it_func =
           list_update_delete_edge_functions.begin();
       it_func != list_update_delete_edge_functions.end(); it_func++) {
    (*it_func)(edge_in);
  }

  edge_in->vertex_src->outgoing_edges.remove(edge_in);
  edge_in->vertex_dst->incoming_edges.remove(edge_in);

  delete edge_in;

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::insert_trajectory(
    vertex_t *vertex_src_in, trajectory_t *trajectory_in,
    std::list<State *> *intermediate_vertices_in, vertex_t *vertex_dst_in) {

  // TODO: take the intermediate vertices into account

  vertex_t *vertex_dst = vertex_dst_in;

  // If no vertex_dst_in is given, then create a new
  //   vertex using the final state in trajectory_in
  if (vertex_dst == NULL) {
    State *final_state = trajectory_in->list_states.back();

    vertex_dst = new vertex_t;
    vertex_dst->state = final_state;

    this->insert_vertex(vertex_dst); // Insert the new vertex into the graph
  }
  trajectory_in->list_states.pop_back();

  // Create the new edge
  edge_t *edge = new edge_t;
  edge->trajectory_edge = trajectory_in;
  this->insert_edge(vertex_src_in, edge,
                    vertex_dst); // Insert the new edge into the graph

  if (intermediate_vertices_in)
    delete intermediate_vertices_in;

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::insert_trajectories(
    vertex_t *vertex_src_in, std::list<trajectory_t *> *list_trajectories_in,
    vertex_t *vertex_dst_in) {

  // Exit if there no trajectories in the list
  if (list_trajectories_in->size() == 0)
    return 1;

  // Initialize the previous vertex
  vertex_t *vertex_prev = vertex_src_in;

  for (typename std::list<trajectory_t *>::iterator iter =
           list_trajectories_in->begin();
       iter != list_trajectories_in->end(); iter++) {

    // Get current trajectory
    trajectory_t *trajectory_curr = *iter;

    // Extract final state from the trajectory
    State *final_state = trajectory_curr->list_states.back();
    trajectory_curr->list_states.pop_back();

    // Create the edge data structure
    edge_t *edge_curr = new edge_t;

    // Create the vertex data structure
    vertex_t *vertex_curr;
    // If this is the last trajectory and connection to a final vertex is
    // required
    iter++;
    if ((iter == list_trajectories_in->end()) && (vertex_dst_in != 0)) {
      vertex_curr = vertex_dst_in;
    } else { // Otherwise create a new vertex
      vertex_curr = new vertex_t;
      vertex_curr->state = final_state;

      // Insert the new vertex into the graph
      this->insert_vertex(vertex_curr);
    }
    iter--;

    // Insert the new edge into the graph
    edge_curr->trajectory = trajectory_curr;
    this->insert_edge(vertex_prev, edge_curr, vertex_curr);

    // Update the previous vertex
    vertex_prev = vertex_curr;
  }

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::init_sampler(sampler_t &sampler_in) {

  sampler = sampler_in;

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::init_distance_evaluator(
    distance_evaluator_t &distance_evaluator_in) {

  distance_evaluator = distance_evaluator_in;

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::init_extender(extender_t &extender_in) {

  extender = extender_in;

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::init_collision_checker(
    collision_checker_t &collision_checker_in) {

  collision_checker = collision_checker_in;

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::init_model_checker(
    model_checker_t &model_checker_in) {

  model_checker = model_checker_in;

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State,
                        Input>::clear_update_function_list_vertex_insert() {

  list_update_insert_vertex_functions.clear();

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::
    register_new_update_function_vertex_insert(
        vertex_update_func_t vertex_update_func_in) {

  list_update_insert_vertex_functions.push_back(vertex_update_func_in);

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State,
                        Input>::clear_update_function_list_vertex_delete() {

  list_update_delete_vertex_functions.clear();

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::
    register_new_update_function_vertex_delete(
        vertex_update_func_t vertex_update_func_in) {

  list_update_delete_vertex_functions.push_back(vertex_update_func_in);

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State,
                        Input>::clear_update_function_list_edge_insert() {

  list_update_insert_edge_functions.clear();

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::register_new_update_function_edge_insert(
    edge_update_func_t edge_update_func_in) {

  list_update_insert_edge_functions.push_back(edge_update_func_in);

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State,
                        Input>::clear_update_function_list_edge_delete() {

  list_update_delete_edge_functions.clear();

  return 1;
}

template <class State, class Input>
int smp::planners::Base<State, Input>::register_new_update_function_edge_delete(
    edge_update_func_t edge_update_func_in) {

  list_update_delete_edge_functions.push_back(edge_update_func_in);

  return 1;
}

#endif
