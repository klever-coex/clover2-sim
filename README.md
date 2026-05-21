# clover2-sim

![Ubuntu 24.04](https://img.shields.io/badge/Ubuntu-24.04-green)
![Windows](https://img.shields.io/badge/Windows-untested-lightgrey)

SITL workspace for [Clover2](https://github.com/klever-coex/clover2) — ROS 2 autonomous quadcopter framework. Launches PX4 in simulation with full visual navigation stack (ArUco, optical flow) using Gazebo Harmonic.

## Quick start

> **Warning**
> ROS 2 Jazzy and Gazebo Harmonic must be installed on the system before building.

```bash
git clone git@github.com:klever-coex/clover2-sim.git
cd clover2-sim
make init    # submodules + vcs import + rosdep
make build   # colcon build --symlink-install
```

## Tech Stack

| Component | Version                           |
| --------- | --------------------------------- |
| ROS 2     | Jazzy                             |
| Gazebo    | Harmonic                          |
| PX4       | v1.16.1 via CMake ExternalProject |
| Bridge    | `ros_gz_bridge`, MAVROS           |

## Commands

```bash
# Build
make build
colcon build --symlink-install --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build single package
colcon build --packages-up-to clover2_sim

# Build PX4 (ExternalProject)
colcon build --packages-select px4_sim

# Launch simulation (Gazebo + PX4 SITL + Clover2 stack)
ros2 launch clover2_sim gz_simple.launch.py

# Headless (no GUI)
ros2 launch clover2_sim gz_simple.launch.py gui:=false

# Use a different world
ros2 launch clover2_sim gz_simple.launch.py world:=clover2_aruco
```

See `Makefile` for all targets (`init`, `build`, `clean`, `init-git`, `init-repos`, `init-deps`).
