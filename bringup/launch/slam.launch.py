# Copyright 2020 ros2_control Development Team
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from launch import LaunchDescription
from launch.actions import RegisterEventHandler
from launch.event_handlers import OnProcessExit
from launch.substitutions import Command, FindExecutable, PathJoinSubstitution
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch.actions import DeclareLaunchArgument

from ament_index_python.packages import get_package_share_directory

from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

import os


from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
def generate_launch_description():

    package_name='ros_phoenix'

    # """Generate launch description with multiple components."""
    # Conditionally include the container based on the value of the use_container argument
    use_container_arg = DeclareLaunchArgument(
        'use_container',
        default_value='true',
        description='Whether to launch the Phoenix container or not'
    )
    container = ComposableNodeContainer(
        name="PhoenixContainer",
        namespace="",
        package="ros_phoenix",
        executable="phoenix_container",
        parameters=[{"interface": "can0"},{"use_container": True}],
        composable_node_descriptions=[
            ComposableNode(
                package="ros_phoenix",
                plugin="ros_phoenix::TalonSRX",
                name="left_wheel_joint",
                parameters=[{"id": 1}, 
                            {"P": 5.0}, 
                            {"I": 0.0}, 
                            {"D":0.0},
                            {"sensor_multiplier":1.0},
                            {"edges_per_rot":4096}],
            ),
            ComposableNode(
                package="ros_phoenix",
                plugin="ros_phoenix::TalonSRX",
                name="right_wheel_joint",
                parameters=[{"id": 3},
                            {"P": 10.0}, 
                            {"I": 0.0}, 
                            {"D":0.0},
                            {"sensor_multiplier":-1.05},
                            {"edges_per_rot":4096}],
            ),
        ],
        condition=IfCondition(LaunchConfiguration('use_container')),
        output="screen",
        
    )

    # Get URDF via xacro
    robot_description_content = Command(
        [
            PathJoinSubstitution([FindExecutable(name="xacro")]),
            " ",
            PathJoinSubstitution(
                [FindPackageShare("ros_phoenix"), "urdf", "diffbot.urdf.xacro"]
            ),
            " ",
            
        ]
    )
    #Get robot description file
    robot_description = {"robot_description": robot_description_content}

    #Get Robot controller node
    robot_controllers = PathJoinSubstitution(
        [
            FindPackageShare("ros_phoenix"),
            "config",
            "diffbot_controllers.yaml",
        ]
    )


    control_node = Node(
        package="controller_manager",
        executable="ros2_control_node",
        parameters=[robot_description, robot_controllers],
        output="both",
    )
    robot_state_pub_node = Node(
        package="robot_state_publisher",
        executable="robot_state_publisher",
        output="both",
        parameters=[robot_description],
        remappings=[
            ("/diff_drive_controller/cmd_vel_unstamped", "/cmd_vel"),
        ],
    )

    # Conditionally include the container based on the value of the use_rviz argument
    rviz_config_file = PathJoinSubstitution(
        [FindPackageShare("ros_phoenix"), "rviz", "gz_simulation.rviz"]
    )
    use_rviz_arg =DeclareLaunchArgument(
        'use_rviz',
        default_value='false',
        description='Whether to launch RViz or not'
    )
    rviz_node = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2",
        output="log",
        parameters=[{"use_rviz": False}],
        condition=IfCondition(LaunchConfiguration('use_rviz')),
        arguments=["-d", rviz_config_file],
    )

    joint_state_broadcaster_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["joint_state_broadcaster", "--controller-manager", "/controller_manager"],
    )

    robot_controller_spawner = Node(
        package="controller_manager",
        executable="spawner",
        arguments=["diffbot_base_controller", "--controller-manager", "/controller_manager"],
    )

    # Delay rviz start after `joint_state_broadcaster`
    delay_rviz_after_joint_state_broadcaster_spawner = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=joint_state_broadcaster_spawner,
            on_exit=[rviz_node],
        )
    )

    # Delay start of robot_controller after `joint_state_broadcaster`
    delay_robot_controller_spawner_after_joint_state_broadcaster_spawner = RegisterEventHandler(
        event_handler=OnProcessExit(
            target_action=joint_state_broadcaster_spawner,
            on_exit=[robot_controller_spawner],
        )
    )

    config_ekf = os.path.join(get_package_share_directory(package_name), 'config', 'localization.yaml')

    start_robot_localization_cmd = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[config_ekf,  {'use_sim_time': False}],
    )

    imu_config = os.path.join(get_package_share_directory(package_name),'config','imu.yaml')

    robot_imu = Node(
        package="bno055",
        executable="bno055",
        parameters=[imu_config],
    )

    nodes = [
        use_rviz_arg,
        use_container_arg,
        control_node,
        robot_state_pub_node,
        joint_state_broadcaster_spawner,
        delay_rviz_after_joint_state_broadcaster_spawner,
        delay_robot_controller_spawner_after_joint_state_broadcaster_spawner,
        container,
        start_robot_localization_cmd,
        robot_imu

    ]

    return LaunchDescription(nodes)