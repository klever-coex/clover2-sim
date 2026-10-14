import os
import pathlib

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    OpaqueFunction,
)
from launch.launch_description_sources import (
    AnyLaunchDescriptionSource,
    PythonLaunchDescriptionSource,
)
from launch.substitutions import (
    LaunchConfiguration,
    PathJoinSubstitution,
)


def launch_setup(context, *args, **kwargs):
    # Reading arguments
    use_sim_time = LaunchConfiguration("use_sim_time")
    log_level = LaunchConfiguration("log_level")
    params_file = LaunchConfiguration("params_file")
    sim_type = LaunchConfiguration("sim_type")
    world = LaunchConfiguration("world")
    name = LaunchConfiguration("name")
    enable_main_camera = LaunchConfiguration("enable_main_camera")
    enable_front_camera = LaunchConfiguration("enable_front_camera")
    enable_led_strip = LaunchConfiguration("enable_led_strip")

    spawn_model_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [
                    get_package_share_directory(
                        "clover2_" + sim_type.perform(context) + "_sim"
                    ),
                    "launch",
                    "spawn.launch.py",
                ]
            )
        ),
        launch_arguments={
            "use_sim_time": use_sim_time,
            "log_level": log_level,
            "params_file": params_file,
            "name": name,
            "world": world,
            "enable_main_camera": enable_main_camera,
            "enable_front_camera": enable_front_camera,
            "enable_led_strip": enable_led_strip,
        }.items(),
    )

    clover2_cmd = IncludeLaunchDescription(
        AnyLaunchDescriptionSource(
            PathJoinSubstitution(
                [
                    get_package_share_directory("clover2_bringup"),
                    "launch",
                    "klever5.launch.xml",
                ]
            )
        ),
        launch_arguments={
            "use_sim_time": use_sim_time,
            "log_level": log_level,
            "params_file": params_file,
            "fcu_bridge.fcu_conn": "udp",
            "localization.map_server.map_filename": "simulation.yaml",
            "sensing_pkg": "clover2_" + sim_type.perform(context) + "_sim",
            "main_camera.enable": enable_main_camera,
            "front_camera.enable": enable_front_camera,
            "indication.led_strip": enable_led_strip,
        }.items(),
    )

    return [
        spawn_model_cmd,
        clover2_cmd,
    ]


def generate_launch_description():
    pkg_clover2_sim = get_package_share_directory("clover2_sim")

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
        default_value=PathJoinSubstitution([pkg_clover2_sim, "params", "klever5.yaml"]),
        description="Log level for all nodes",
    )

    sim_type_declare = DeclareLaunchArgument(
        "sim_type",
        default_value="gz",
        choices=["gz"],
        description="Type of simulator for now only gz.",
    )

    world_declare = DeclareLaunchArgument(
        "world",
        default_value="clover2_aruco",
        description="Gazebo world.",
    )

    name_declare = DeclareLaunchArgument(
        "name",
        default_value="px4",
        description="Model name.",
    )
    equipment_declare = [
        DeclareLaunchArgument(
            "enable_main_camera",
            default_value="true",
            choices=["true", "false"],
        ),
        DeclareLaunchArgument(
            "enable_front_camera",
            default_value="false",
            choices=["true", "false"],
        ),
        DeclareLaunchArgument(
            "enable_led_strip",
            default_value="false",
            choices=["true", "false"],
        ),
    ]

    return LaunchDescription(
        [
            use_sim_time_declare,
            log_level_declare,
            params_file_declare,
            sim_type_declare,
            world_declare,
            name_declare,
            *equipment_declare,
            OpaqueFunction(function=launch_setup),
        ]
    )
