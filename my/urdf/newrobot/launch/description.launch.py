"""Launch file that only publishes robot_description and TF.

Use this when you need the robot description available on the parameter server
without launching RViz or joint_state_publisher_gui (e.g., with MoveIt).

Usage:
    ros2 launch newrobot description.launch.py
"""
import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    pkg_path = get_package_share_directory('newrobot')
    xacro_file = os.path.join(pkg_path, 'urdf', 'newrobot.urdf.xacro')

    robot_description = ParameterValue(
        Command(['xacro ', xacro_file]),
        value_type=str
    )

    return LaunchDescription([
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            parameters=[{'robot_description': robot_description}]
        ),
    ])
