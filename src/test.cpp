#define Phoenix_No_WPI // remove WPI dependencies
#include "ctre/phoenix/platform/Platform.h"
#include "ctre/phoenix/unmanaged/Unmanaged.h"

#include "rclcpp/rclcpp.hpp"
#include "ros_phoenix/talon_component.hpp"

int main(int argc, char **argv)
{
    std::string interface = "can0";
    ctre::phoenix::platform::can::SetCANInterface(interface.c_str());

    rclcpp::init(argc, argv);
    auto exec = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    auto node = std::make_shared<ros_phoenix::TalonComponent>(rclcpp::NodeOptions());
    exec->add_node(node);

    while (rclcpp::ok())
    {
        ctre::phoenix::unmanaged::FeedEnable(100);
        exec->spin_once();
        node.reset();
        return 0;
    }
}