import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    LogInfo,
    RegisterEventHandler,
)
from launch.event_handlers import OnProcessExit
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare
from px4_sim.actions import PX4Sitl


def generate_launch_description():
    pkg_clover2_gz_sim = get_package_share_directory("clover2_gz_sim")

    # Reading arguments
    use_sim_time = LaunchConfiguration("use_sim_time")
    log_level = LaunchConfiguration("log_level")
    params_file = LaunchConfiguration("params_file")
    world = LaunchConfiguration("world")
    model = LaunchConfiguration("model")
    name = LaunchConfiguration("name")

    # Declare arguments
    use_sim_time_declare = DeclareLaunchArgument(
        "use_sim_time",
        default_value="true",
        description="Use simulation (Gazebo) clock if true",
    )

    log_level_declare = DeclareLaunchArgument(
        "log_level", default_value="info", description="Log level for all nodes"
    )

    params_file_declare = DeclareLaunchArgument(
        "params_file",
        description="Log level for all nodes",
    )

    world_declare = DeclareLaunchArgument(
        "world",
        description="Gazebo world.",
    )

    model_declare = DeclareLaunchArgument(
        "model",
        description="Select sim model.",
    )

    name_declare = DeclareLaunchArgument(
        "name",
        description="Model name.",
    )

    # Spawn model to gazebo
    spawn_cmd = Node(
        package="ros_gz_sim",
        executable="create",
        output="screen",
        parameters=[
            {
                "world": world,
                "file": PathJoinSubstitution(
                    [pkg_clover2_gz_sim, "models", model, "model.sdf"]
                ),
                "name": name,
                "allow_renaming": False,
                "x": 0.0,
                "y": 0.0,
                "z": 0.5,
                "R": 0.0,
                "P": 0.0,
                "Y": 0.0,
            }
        ],
    )

    px4_run_cmd = PX4Sitl(
        name=name,
        workdir="/tmp/clover2_px4_workdir",
        extra_envs={
            "PX4_GZ_STANDALONE": "1",
            "PX4_GZ_MODEL_NAME": name,
        },
    )

    wait_spawn = ExecuteProcess(cmd=["sleep", "5"])

    return LaunchDescription(
        [
            use_sim_time_declare,
            log_level_declare,
            params_file_declare,
            world_declare,
            model_declare,
            name_declare,
            spawn_cmd,
            RegisterEventHandler(
                OnProcessExit(
                    target_action=spawn_cmd,
                    on_exit=[wait_spawn],
                )
            ),
            RegisterEventHandler(
                OnProcessExit(
                    target_action=wait_spawn,
                    on_exit=[
                        LogInfo(msg="Spawn finished"),
                        px4_run_cmd,
                    ],
                )
            ),
        ]
    )
