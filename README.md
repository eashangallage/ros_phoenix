# luna_rover_ws

## Cloning and Building
1. Clone the package into a ROS2 workspace
```
$ cd <ros_ws>/src
$ git clone git@github.com:eashangallage/ros_phoenix.git
$ cd ros_phoenix
```
2. Build the workspace and source the setup file
```
$ cd <ros_ws>
$ colcon build --symlink-install --packages-select ros_phoenix
$ source install/setup.bash
```
### With RVIZ
```
$ cd <ros_ws>
$ source install/setup.bash
$ ros2 launch ros_phoenix diffbot.launch.py use_rviz:=true
```

## Launch in Physical Rover
```
$ cd <ros_ws>
$ source install/setup.bash
$ ros2 launch ros_phoenix diffbot.launch.py
```
## Launch in Simulation
```
$ cd <ros_ws>
$ source install/setup.bash
$ ros2 launch ros_phoenix launch_sim.launch.py
```
### With RVIZ
```
$ cd <ros_ws>
$ source install/setup.bash
$ ros2 launch ros_phoenix launch_sim.launch.py use_rviz:=true
```
