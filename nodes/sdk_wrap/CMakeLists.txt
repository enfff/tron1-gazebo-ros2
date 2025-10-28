cmake_minimum_required(VERSION 3.8)
project(sdk_wrap)

# Default to C++17
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(limxsdk_lowlevel REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2 REQUIRED)
find_package(std_msgs REQUIRED)

include_directories(
  ${limxsdk_lowlevel_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
)

#include_directories(
#  /root/limx_ws/src/limxsdk-lowlevel/include
#  /root/limx_ws/install/limxsdk_lowlevel/include
#)

add_executable(imu_publisher src/imu_publisher_node.cpp)
ament_target_dependencies(imu_publisher rclcpp sensor_msgs)
# Link the limxsdk_lowlevel library exported by the package
if(limxsdk_lowlevel_LIBRARIES)
  target_link_libraries(imu_publisher ${limxsdk_lowlevel_LIBRARIES})
endif()

add_executable(odom_publisher src/odom_publisher_node.cpp)
ament_target_dependencies(odom_publisher rclcpp nav_msgs)
# Link WebSocket++ and related libraries: -lssl -lcrypto -lboost_system -lpthread
target_link_libraries(odom_publisher 
  ${Boost_LIBRARIES}
  OpenSSL::SSL
  OpenSSL::Crypto
  pthread
)

add_executable(robot_command src/robot_command.cpp)
ament_target_dependencies(robot_command rclcpp geometry_msgs)
if(limxsdk_lowlevel_LIBRARIES)
  target_link_libraries(robot_command ${limxsdk_lowlevel_LIBRARIES})
endif()

add_executable(joint_state_publisher src/joint_state_publisher_node.cpp)
ament_target_dependencies(joint_state_publisher rclcpp sensor_msgs)
if(limxsdk_lowlevel_LIBRARIES)
  target_link_libraries(joint_state_publisher ${limxsdk_lowlevel_LIBRARIES})
endif()

add_executable(livox_lidar_node src/livox_lidar_node.cpp)
ament_target_dependencies(livox_lidar_node 
  rclcpp 
  sensor_msgs 
  geometry_msgs
  tf2_ros
  tf2
  std_msgs
)

install(TARGETS imu_publisher odom_publisher robot_command joint_state_publisher livox_lidar_node
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME})

ament_package()
