cmake_minimum_required(VERSION 3.8)
project(imu_publisher)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(limxsdk_lowlevel REQUIRED)

include_directories(
  ${limxsdk_lowlevel_INCLUDE_DIRS}
)

add_executable(imu_publisher_node src/imu_publisher_node.cpp)
ament_target_dependencies(imu_publisher_node rclcpp sensor_msgs)
# Link the limxsdk_lowlevel library exported by the package
if(limxsdk_lowlevel_LIBRARIES)
  target_link_libraries(imu_publisher_node ${limxsdk_lowlevel_LIBRARIES})
endif()

install(TARGETS imu_publisher_node
  DESTINATION lib/${PROJECT_NAME})

ament_package()
