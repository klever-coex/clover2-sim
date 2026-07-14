import os
from typing import List, Optional

from ament_index_python.packages import get_package_share_directory
from launch import Action
from launch.actions import ExecuteProcess
from launch.launch_context import LaunchContext
from launch.launch_description_entity import LaunchDescriptionEntity
from launch.some_substitutions_type import SomeSubstitutionsType
from launch.utilities import perform_substitutions
from launch.utilities.type_utils import normalize_to_list_of_substitutions


class PX4Sitl(Action):
    def __init__(
        self,
        *,
        name: SomeSubstitutionsType,
        workdir: Optional[SomeSubstitutionsType] = None,
        autostart: Optional[SomeSubstitutionsType] = "4001",
        extra_envs: Optional[dict] = None,
        **kwargs,
    ) -> None:
        super().__init__(**kwargs)

        if not workdir:
            workdir = os.path.join(get_package_share_directory("px4_sim"), "workdir")

        extra_envs = extra_envs or {}

        self.__name = normalize_to_list_of_substitutions(name)
        self.__workdir = normalize_to_list_of_substitutions(workdir)
        self.__autostart = normalize_to_list_of_substitutions(autostart)
        self.__extra_envs = {
            k: normalize_to_list_of_substitutions(v) for k, v in extra_envs.items()
        }

    def execute(
        self, context: LaunchContext
    ) -> Optional[List[LaunchDescriptionEntity]]:
        pkg_share = get_package_share_directory("px4_sim")

        name = perform_substitutions(context, self.__name)
        workdir = perform_substitutions(context, self.__workdir)
        autostart = perform_substitutions(context, self.__autostart)

        resolved_extra_envs = {
            k: perform_substitutions(context, v) for k, v in self.__extra_envs.items()
        }

        os.makedirs(workdir, exist_ok=True)
        named_workdir = os.path.join(workdir, name)

        cmd = ["px4", "-d", f"{pkg_share}/etc", "-w", named_workdir]
        proc = ExecuteProcess(
            cmd=cmd,
            cwd=pkg_share,
            additional_env={
                "PX4_SYS_AUTOSTART": autostart,
                **resolved_extra_envs,
            },
            output="screen",
        )
        return [proc]
