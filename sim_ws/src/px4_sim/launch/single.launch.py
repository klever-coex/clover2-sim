import os
import tempfile
from distutils.dir_util import copy_tree

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    ExecuteProcess,
)
from launch.substitutions import (
    LaunchConfiguration,
    PathJoinSubstitution,
)


def seed_rootfs(rootfs):
    px4_dir = get_package_share_directory('px4_sim')
    print(f'seeding rootfs at {rootfs} from {px4_dir}')
    copy_tree(px4_dir, rootfs)

def generate_launch_description():
    pkg_px4_sim = get_package_share_directory('px4_sim')

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

    name_declare = DeclareLaunchArgument(
        "name",
        default_value="px4",
        description="Model name.",
    )

    use_sim_time = LaunchConfiguration("use_sim_time")
    log_level = LaunchConfiguration("log_level")
    params_file = LaunchConfiguration("params_file")
    sim_type = LaunchConfiguration("sim_type")
    world = LaunchConfiguration("world")
    model = LaunchConfiguration("model")
    name = LaunchConfiguration("name")

    # rootfs = tempfile.TemporaryDirectory()
    rc_script = os.path.join(pkg_px4_sim, 'etc/init.d-posix/rcS')
    # print('using rootfs ', rootfs.name)
    # seed_rootfs(rootfs.name)
    # rootfs = "/home/motya/own_projects/coex/clover2-sim"

    cmd = ['px4', '%s/ROMFS/px4fmu_common' % pkg_px4_sim,
           '-d', "%s/etc" % pkg_px4_sim]

    px4_cmd = ExecuteProcess(
        cmd=cmd,
        cwd=pkg_px4_sim,
        additional_env={
            "PX4_GZ_STANDALONE": "1",
            "PX4_GZ_MODEL_NAME": name,
            "PX4_SYS_AUTOSTART": "4001",
            "GZ_PARTITION": "clover2",
        },
        output='screen')

    return LaunchDescription([
        use_sim_time_declare,
        log_level_declare,
        params_file_declare,
        name_declare,
        px4_cmd,
    ])
