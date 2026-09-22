cmake_minimum_required(VERSION 3.8)
project(px4_sim)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

ament_python_install_package(${PROJECT_NAME})

set(PX4_PREBUILT_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/px4_sitl_default)
set(PX4_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/px4-autopilot)

if(EXISTS "${PX4_PREBUILT_DIR}")
  install(
    DIRECTORY
      ${PX4_PREBUILT_DIR}/bin
      ${PX4_PREBUILT_DIR}/etc
      etc
    DESTINATION share/${PROJECT_NAME}
    USE_SOURCE_PERMISSIONS
  )
elseif(EXISTS "${PX4_SOURCE_DIR}/Makefile" AND
       EXISTS "${PX4_SOURCE_DIR}/CMakeLists.txt")
  set(PX4_BUILD_DIR ${PX4_SOURCE_DIR}/build/px4_sitl_default)

  add_custom_target(px4_build ALL
    COMMAND ${CMAKE_COMMAND} -E env MAKEFLAGS=$ENV{MAKEFLAGS}
            ${CMAKE_MAKE_PROGRAM} px4_sitl_default
    WORKING_DIRECTORY ${PX4_SOURCE_DIR}
    USES_TERMINAL
  )

  install(DIRECTORY
    ${PX4_BUILD_DIR}/bin ${PX4_BUILD_DIR}/etc etc
    DESTINATION share/${PROJECT_NAME}
  )
else()
  message(FATAL_ERROR
    "PX4 not found: provide ${PX4_PREBUILT_DIR} or init ${PX4_SOURCE_DIR} submodule"
  )
endif()

ament_environment_hooks(cmake/99_setup.sh.in)

ament_package()
