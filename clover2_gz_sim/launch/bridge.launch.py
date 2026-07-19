from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from ros_gz_bridge.actions import RosGzBridge


def generate_launch_description():
    pkg_clover2_gz_sim = get_package_share_directory("clover2_gz_sim")

    use_sim_time_declare = DeclareLaunchArgument(
        "use_sim_time",
        default_value="true",
        description="Use simulation (Gazebo) clock if true",
    )

    bridge_name_declare = DeclareLaunchArgument(
        "bridge_name",
        default_value="gz_common_bridge",
        description="Name of the ros_gz_bridge node",
    )

    config_file_declare = DeclareLaunchArgument(
        "config_file",
        default_value=pkg_clover2_gz_sim + "/config/gz_common_bridge.yaml",
        description="YAML config file for bridge mappings",
    )

    ros_gz_bridge_action = RosGzBridge(
        bridge_name=LaunchConfiguration("bridge_name"),
        config_file=LaunchConfiguration("config_file"),
    )

    return LaunchDescription(
        [
            use_sim_time_declare,
            bridge_name_declare,
            config_file_declare,
            ros_gz_bridge_action,
        ]
    )
