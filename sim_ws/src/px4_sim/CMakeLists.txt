cmake_minimum_required(VERSION 3.8)
project(px4_sim)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)

include(ExternalProject)

find_program(MAKE_EXECUTABLE NAMES make REQUIRED)

# externalproject_add(px4-firmware
#   PREFIX
#       ${PROJECT_BINARY_DIR}/px4-src
#   GIT_PROGRESS ON
#   GIT_REPOSITORY
#     https://github.com/PX4/PX4-Autopilot
#   GIT_TAG
#     94cb201
#   INSTALL_COMMAND
#     mkdir -p ${CMAKE_INSTALL_PREFIX}/share/px4_sim &&
#     rsync -avL --exclude .git --exclude build
#       build/px4_sitl_default/bin
#       Tools/simulation/gz/models
#       build/px4_sitl_default/etc
#       ${CMAKE_INSTALL_PREFIX}/share/px4_sim &&
#   BUILD_COMMAND ${MAKE_EXECUTABLE} px4_sitl_default -j8
#   BUILD_IN_SOURCE OFF
#   CONFIGURE_COMMAND ""
#   SOURCE_DIR ${CMAKE_SOURCE_DIR}/px4-src
# )

# externalproject_add(px4-firmware
#   PREFIX
#       ${PROJECT_BINARY_DIR}/px4-src
#   GIT_REPOSITORY
#     https://github.com/PX4/PX4-Autopilot
#   GIT_TAG
#     94cb201
#   INSTALL_COMMAND
#     mkdir -p ${CMAKE_INSTALL_PREFIX}/share/px4_sim &&
#     rsync -avL --exclude .git --exclude build
#       build/px4_sitl_default/bin
#       Tools/simulation/gz/models
#       build/px4_sitl_default/etc
#       ${CMAKE_INSTALL_PREFIX}/share/px4_sim
#   BUILD_COMMAND ${MAKE_EXECUTABLE} px4_ros2 -j && make px4_sitl_default -j
#   BUILD_IN_SOURCE ON
#   CONFIGURE_COMMAND ""
# )

add_custom_target(px4_build ALL
  COMMAND ${CMAKE_COMMAND} -E echo "Checking PX4 build..."
  COMMAND bash -c "
    if [ ! -f ${PX4_BINARY} ]; then
      echo 'Building PX4...';
      make px4_sitl_default -j$(nproc);
    else
      echo 'PX4 already built';
    fi
  "
  WORKING_DIRECTORY ${PX4_SOURCE_DIR}
)

install(DIRECTORY
  launch
  DESTINATION
    share/${PROJECT_NAME}
)

# ament_environment_hooks(cmake/99_px4_setup.sh.in)

ament_export_libraries(px4)

ament_package()
