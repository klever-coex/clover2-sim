import os

from ament_index_python.packages import get_package_prefix, get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    OpaqueFunction,
    SetEnvironmentVariable,
    LogInfo,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import (
    EnvironmentVariable,
    LaunchConfiguration,
    PathJoinSubstitution,
)
from launch_ros.substitutions import FindPackageShare


def launch_gazebo(context, *args, **kwargs):
    pkg_clover2_gz_sim = get_package_share_directory("clover2_gz_sim")

    # Let Gazebo find our rangefinder system plugin installed in the package's lib directory
    plugin_dir = os.path.join(get_package_prefix("clover2_gz_sim"), "lib")
    plugin_paths = os.pathsep.join(
        [plugin_dir, os.environ.get("GZ_SIM_SYSTEM_PLUGIN_PATH", "")]
    )
    plugin_path_env = SetEnvironmentVariable("GZ_SIM_SYSTEM_PLUGIN_PATH", plugin_paths)

    use_sim_time = LaunchConfiguration("use_sim_time")
    log_level = LaunchConfiguration("log_level")
    world = LaunchConfiguration("world")
    gui = LaunchConfiguration("gui").perform(context)
    render_engine = LaunchConfiguration("render_engine").perform(context)

    log = LogInfo(
        msg=f"Render engine: {render_engine if render_engine else 'empty (means ogre2)'}"
    )

    gz_args = [
        os.path.join(pkg_clover2_gz_sim, "worlds/"),
        world,
        ".sdf",
        " -v 2",
        " -r",
    ]
    if gui == "false":
        gz_args += [" --headless-rendering", " -s"]
    if render_engine:
        gz_args += [" --render-engine ", render_engine]

    gazebo_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [FindPackageShare("ros_gz_sim"), "launch", "gz_sim.launch.py"]
            )
        ),
        launch_arguments={
            "use_sim_time": use_sim_time,
            "log_level": log_level,
            "gz_args": gz_args,
        }.items(),
    )

    return [log, plugin_path_env, gazebo_cmd]


def generate_launch_description():
    use_sim_time = LaunchConfiguration("use_sim_time")

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
        default_value="clover2_aruco",
        description="Gazebo world.",
    )

    gui_declare = DeclareLaunchArgument(
        "gui",
        default_value="true",
        choices=["true", "false"],
        description='Set to "false" to run headless.',
    )

    render_engine_declare = DeclareLaunchArgument(
        "render_engine",
        default_value=EnvironmentVariable(
            "CLOVER2_GZ_SIM_RENDER_ENGINE", default_value=""
        ),
        description="Gazebo rendering engine override (defaults to Gazebo configuration).",
    )

    gz_common_bridge_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution(
                [FindPackageShare("clover2_gz_sim"), "launch", "bridge.launch.py"]
            )
        ),
        launch_arguments={
            "use_sim_time": use_sim_time,
        }.items(),
    )

    return LaunchDescription(
        [
            use_sim_time_declare,
            log_level_declare,
            # params_file_declare,
            world_declare,
            gui_declare,
            render_engine_declare,
            OpaqueFunction(function=launch_gazebo),
            gz_common_bridge_cmd,
        ]
    )
