# clover2-sim

![Ubuntu 24.04](https://img.shields.io/badge/Ubuntu-24.04-green)
![Windows](https://img.shields.io/badge/Windows-untested-lightgrey)

SITL workspace for [Clover2](https://github.com/klever-coex/clover2) - ROS 2 autonomous quadcopter framework. Launches PX4 in simulation with full visual navigation stack (ArUco, optical flow) using Gazebo Harmonic.

## Quick start

> **Warning**
> ROS 2 Jazzy and Gazebo Harmonic must be installed on the system before building. Or use [clover2-dev](https://github.com/klever-coex/clover2-dev).

## Drone models

- `x500_mono_cam_down`: downward-facing camera and GPU lidar rangefinder for Ogre2.
- `x500_mono_cam_down_raycast`: downward-facing camera and physics raycast rangefinder for Ogre1. This is the default model in `gz_simple.launch.py`, matching the Ogre1 sensors configuration in `clover2_aruco.sdf`. The rangefinder requires DART's Bullet collision detector, configured in that world.

Select a model with the `model` launch argument. Both rangefinders publish to `/rangefinder`.

## Tech Stack

| Component | Version                           |
| --------- | --------------------------------- |
| ROS 2     | Jazzy                             |
| Gazebo    | Harmonic                          |
| PX4       | v1.16.1 via CMake ExternalProject |
| Bridge    | `ros_gz_bridge`, MAVROS           |
