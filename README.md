# clover2-sim

![Ubuntu 24.04](https://img.shields.io/badge/Ubuntu-24.04-green)
![Windows](https://img.shields.io/badge/Windows-untested-lightgrey)

SITL workspace for [Clover2](https://github.com/klever-coex/clover2) - ROS 2 autonomous quadcopter framework. Launches PX4 in simulation with full visual navigation stack (ArUco, optical flow) using Gazebo Harmonic.

## Quick start

> **Warning**
> ROS 2 Jazzy and Gazebo Harmonic must be installed on the system before building. Or use [clover2-dev](https://github.com/klever-coex/clover2-dev).

## Tech Stack

| Component | Version                           |
| --------- | --------------------------------- |
| ROS 2     | Jazzy                             |
| Gazebo    | Harmonic                          |
| PX4       | v1.16.1 via CMake ExternalProject |
| Bridge    | `ros_gz_bridge`, MAVROS           |
