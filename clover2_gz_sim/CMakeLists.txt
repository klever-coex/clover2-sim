cmake_minimum_required(VERSION 3.8)
project(clover2_gz_sim)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(gz-sim8 REQUIRED)

add_library(clover2-rangefinder-system SHARED
  src/rangefinder_system.cpp
)
target_compile_features(clover2-rangefinder-system PUBLIC cxx_std_17)
target_link_libraries(clover2-rangefinder-system
  PRIVATE gz-sim8::gz-sim8
)

install(
  TARGETS clover2-rangefinder-system
  LIBRARY DESTINATION lib
)

ament_environment_hooks(
  env-hooks/gz_sim_system_plugin_path.dsv
)

install(
  DIRECTORY worlds launch models config
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
