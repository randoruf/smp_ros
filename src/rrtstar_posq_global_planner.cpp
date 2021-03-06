/*
 * Copyright (C) 2018 Sertac Karaman
 * Copyright (C) 2018 Chittaranjan Srinivas Swaminathan
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

#include <pluginlib/class_list_macros.h>

#include <smp_ros/rrtstar_posq_global_planner.hpp>

std::array<double, 3> distanceBetweenStates(const std::array<double, 3> &state,
                                            const std::array<double, 3> &goal);
void mrptMapFromROSMsg(
    const std::shared_ptr<mrpt::maps::COccupancyGridMap2D> &map,
    const costmap_2d::Costmap2D *rosMap);

namespace smp_ros {

void RRTStarPosQGlobalPlanner::initialize(
    std::string name, costmap_2d::Costmap2DROS *costmap_ros) {

  // TODO: All parameters must be configurable.
  graph_pub = nh.advertise<geometry_msgs::PoseArray>("/graph", 100);

  map = std::make_shared<mrpt::maps::COccupancyGridMap2D>();
  mrptMapFromROSMsg(map, costmap_ros->getCostmap());

  std::vector<geometry_msgs::Point> footprint_pt =
      costmap_ros->getRobotFootprint();
  footprint = std::make_shared<mrpt::math::CPolygon>();

  for (const auto &point : footprint_pt) {
    footprint->AddVertex(point.x, point.y);
  }

  if (footprint_pt.size() != 4) {
    ROS_WARN("Footprint wasn't a polygon. Setting to default values.");
    footprint->AddVertex(0.25, 0.125);
    footprint->AddVertex(0.25, -0.125);
    footprint->AddVertex(-0.25, 0.125);
    footprint->AddVertex(-0.25, -0.125);
  } else {
    ROS_INFO("RRTStarPosQGlobalPlanner got a polygon footprint.");
  }

  // TODO: Inflation radius and footprint must be configurable.
  collision_checker =
      std::make_shared<smp::collision_checkers::MultipleCirclesMRPT<State>>(
          map, 0.15, footprint);

  // Sampler support should also be configurable.
  smp::Region<3> sampler_support;
  sampler_support.center[0] = 0.0;
  sampler_support.size[0] = 10.0;
  sampler_support.center[1] = 0.0;
  sampler_support.size[1] = 10.0;
  sampler_support.center[2] = 0.0;
  sampler_support.size[2] = 2 * 3.14;
  sampler.set_support(sampler_support);
}

bool RRTStarPosQGlobalPlanner::makePlan(
    const geometry_msgs::PoseStamped &start,
    const geometry_msgs::PoseStamped &goal,
    std::vector<geometry_msgs::PoseStamped> &plan) {

  smp::distance_evaluators::KDTree<State, Input, 3> distance_evaluator;
  smp::multipurpose::MinimumTimeReachability<State, Input, 3>
      min_time_reachability;

  smp::planners::RRTStar<State, Input> planner(
      sampler, distance_evaluator, extender, *collision_checker,
      min_time_reachability, min_time_reachability);

  planner.parameters.set_phase(2);
  planner.parameters.set_gamma(std::max(map->getXMax(), map->getYMax()));
  planner.parameters.set_dimension(3);
  planner.parameters.set_max_radius(10.0);

  ROS_INFO("Start: (%lf,%lf,%lf degrees)", start.pose.position.x,
           start.pose.position.y,
           2 * atan2(start.pose.orientation.z, start.pose.orientation.w) * 180 /
               M_PI);

  ROS_INFO("Going to goal: (%lf,%lf,%lf degrees)", goal.pose.position.x,
           goal.pose.position.y,
           2 * atan2(goal.pose.orientation.z, goal.pose.orientation.w) * 180 /
               M_PI);

  ros::Publisher path_pub = nh.advertise<geometry_msgs::PoseArray>("/path", 10);

  smp::Region<3> region_goal;
  region_goal.center[0] = goal.pose.position.x;
  region_goal.size[0] = 0.75;

  region_goal.center[1] = goal.pose.position.y;
  region_goal.size[1] = 0.75;

  region_goal.center[2] =
      2 * atan2(goal.pose.orientation.z, goal.pose.orientation.w);
  region_goal.size[2] = 0.2;

  min_time_reachability.set_goal_region(region_goal);
  min_time_reachability.set_distance_function(distanceBetweenStates);

  sampler.set_goal_bias(0.05, region_goal);

  State *state_initial = new State;

  state_initial->state_vars[0] = start.pose.position.x;
  state_initial->state_vars[1] = start.pose.position.y;
  state_initial->state_vars[2] =
      2 * atan2(start.pose.orientation.z, start.pose.orientation.w);

  if (collision_checker->check_collision(state_initial) == 0) {
    ROS_INFO("Start state is in collision. Planning failed.");
    return false;
  } else
    ROS_INFO("Start state is not in collision.");

  planner.initialize(state_initial);

  ros::Time t = ros::Time::now();
  // 3. RUN THE PLANNER
  int i = 0;
  double planning_time = 5.0;

  graph.header.frame_id = "map";
  while (ros::ok()) {
    ++i;
    if (ros::Time::now() - t > ros::Duration(planning_time)) {
      ROS_INFO("Planning time of %lf sec. elapsed.", planning_time);
      break;
    }

    planner.iteration();

    graph.poses.clear();

    graphToMsg(nh, graph, planner.get_root_vertex());
    graph.header.stamp = ros::Time::now();

    graph_pub.publish(graph);
    ROS_INFO_THROTTLE(1.0, "Planner iteration : %d", i);
  }

  Trajectory trajectory_final;
  min_time_reachability.get_solution(trajectory_final);

  geometry_msgs::PoseArray path;

  for (const auto &state : trajectory_final.list_states) {
    geometry_msgs::Pose p;
    p.position.x = (*state)[0];
    p.position.y = (*state)[1];
    p.orientation.z = sin((*state)[2] / 2);
    p.orientation.w = cos((*state)[2] / 2);
    path.poses.push_back(p);

    geometry_msgs::PoseStamped pose;
    pose.pose.position.x = (*state)[0];
    pose.pose.position.y = (*state)[1];
    pose.pose.orientation.z = sin((*state)[2] / 2);
    pose.pose.orientation.w = cos((*state)[2] / 2);
    plan.push_back(pose);
  }

  int k = 0;
  for (const auto &time : trajectory_final.list_inputs) {
    plan[k].header.frame_id = "map";
    plan[k].header.stamp = ros::Time(0) + ros::Duration((*time)[0]);
    k++;
  }

  path.header.stamp = ros::Time::now();
  path.header.frame_id = "map";
  path_pub.publish(path);

  sleep(2);

  return true;
}
} // namespace smp_ros

PLUGINLIB_EXPORT_CLASS(smp_ros::RRTStarPosQGlobalPlanner,
                       nav_core::BaseGlobalPlanner)
