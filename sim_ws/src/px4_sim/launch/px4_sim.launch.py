import os
import tempfile
from distutils.dir_util import copy_tree

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess


def seed_rootfs(rootfs):
    px4_dir = get_package_share_directory('px4_sim')
    print(f'seeding rootfs at {rootfs} from {px4_dir}')
    copy_tree(px4_dir, rootfs)


def get_px4_process(precommnad=None):
    px4_sim_dir = get_package_share_directory('px4_sim')

    rootfs = tempfile.TemporaryDirectory()
    rc_script = os.path.join(px4_sim_dir, 'etc/init.d-posix/rcS')
    print('using rootfs ', rootfs.name)
    seed_rootfs(rootfs.name)

    cmd = ['px4', '%s/ROMFS/px4fmu_common' % rootfs.name,
           '-s', rc_script,
           '-d']

    run_px4 = ExecuteProcess(
        cmd=cmd,
        cwd=px4_sim_dir,
        output='screen')

    return run_px4


def generate_launch_description():

    px4_cmd = get_px4_process()

    return LaunchDescription([
        px4_cmd
    ])
