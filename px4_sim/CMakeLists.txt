cmake_minimum_required(VERSION 3.8)
project(px4_sim)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

ament_python_install_package(${PROJECT_NAME})

include(ExternalProject)

set(PX4_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/px4-autopilot)
set(PX4_BUILD_DIR ${PX4_SOURCE_DIR}/build/px4_sitl_default)

add_custom_target(px4_build ALL
  COMMAND ${CMAKE_COMMAND} -E env MAKEFLAGS=$ENV{MAKEFLAGS}
          ${CMAKE_MAKE_PROGRAM} px4_sitl_default
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/px4-autopilot
  USES_TERMINAL
)

install(DIRECTORY
  ${PX4_BUILD_DIR}/bin/
  DESTINATION share/${PROJECT_NAME}/bin
  USE_SOURCE_PERMISSIONS
)

install(DIRECTORY
  ${PX4_BUILD_DIR}/etc etc
  DESTINATION share/${PROJECT_NAME}
)

ament_environment_hooks(cmake/99_setup.sh.in)

ament_export_libraries(px4)

ament_package()
