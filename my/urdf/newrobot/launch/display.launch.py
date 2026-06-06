from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():
    pkg_path = get_package_share_directory('newrobot')
    with open(os.path.join(pkg_path, 'urdf', 'newrobot.urdf'), 'r') as f:
        robot_desc = f.read()

    return LaunchDescription([
        Node(package='joint_state_publisher_gui', executable='joint_state_publisher_gui'),
        Node(package='robot_state_publisher', executable='robot_state_publisher',
             parameters=[{'robot_description': robot_desc}]),
        Node(package='rviz2', executable='rviz2'),
    ])
