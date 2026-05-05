import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
)
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare
from launch.launch_description_sources import PythonLaunchDescriptionSource


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

    spawn_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [FindPackageShare("ros_gz_sim"), "launch", "gz_spawn_model.launch.py"]
            )
        ),
        launch_arguments={
            "world": world,
            "file": PathJoinSubstitution([pkg_clover2_gz_sim, "models", model, "model.sdf"]),
            "entity_name": name,
            "allow_renaming": "false"
        }.items(),
    )

    return LaunchDescription(
        [
            use_sim_time_declare,
            log_level_declare,
            params_file_declare,
            world_declare,
            model_declare,
            name_declare,
            spawn_cmd,
        ]
    )
