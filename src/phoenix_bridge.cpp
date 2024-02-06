/*
Author      Date        Description
K Nguyen    2-3-2024    - Renamed configure to on_init and change return type to CallbackReturn
                        - If using BaseInterface as base class then you should remove it.
                        - replaced first three lines in on_init to
                            if (hardware_interface::[Actuator|Sensor|System]Interface::on_init(info) != CallbackReturn::SUCCESS)
                            {
                            return CallbackReturn::ERROR;
                            }
                        - Changed last return of on_init to return CallbackReturn::SUCCESS;
                        -Renamed start() to on_activate(const rclcpp_lifecycle::State & previous_state) 
                            and stop() to on_deactivate(const rclcpp_lifecycle::State & previous_state)
                        - Change return type of on_activate and on_deactivate to CallbackReturn    
                        -Change last return of on_activate and on_deactivate to return CallbackReturn::SUCCESS;
            2-5-2024    -Changed read/write() to read/write(const rclcpp::Time & time, const rclcpp::Duration & period)

                        */

#include "ros_phoenix/phoenix_bridge.hpp"

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rcutils/logging_macros.h"

namespace ros_phoenix {

PhoenixBridge::InterfaceType PhoenixBridge::str_to_interface(const std::string& str)
{
    if (str == "percent_output") {
        return InterfaceType::PERCENT_OUTPUT;

    } else if (str == hardware_interface::HW_IF_POSITION) {
        return InterfaceType::POSITION;

    } else if (str == hardware_interface::HW_IF_VELOCITY) {
        return InterfaceType::VELOCITY;

    } else {
        return InterfaceType::INVALID;
    }
}

PhoenixBridge::PhoenixBridge()
    : logger_(rclcpp::get_logger("PhoenixBridge"))
{
}

// hardware_interface::return_type PhoenixBridge::configure(
hardware_interface::CallbackReturn PhoenixBridge::on_init(
    const hardware_interface::HardwareInfo& info)
{
    this->logger_ = rclcpp::get_logger(info.name);
    this->info_ = info;

    // if (configure_default(info) != hardware_interface::return_type::OK) {
    //     // return hardware_interface::return_type::ERROR;
    //     return hardware_interface::CallbackReturn::ERROR;

    // }
    if (hardware_interface::SystemInterface::on_init(info) != CallbackReturn::SUCCESS)
    {
      return CallbackReturn::ERROR;
    }

    for (auto joint : info.joints) {
        auto cmd = std::make_shared<ros_phoenix::msg::MotorControl>();

        if (joint.command_interfaces.size() != 1) {
            // RCLCPP_FATAL(this->logger_, "Joint '%s' has %d command interfaces. Expected 1.",
            //     joint.name.c_str(), joint.command_interfaces.size());
            RCLCPP_FATAL(this->logger_, "Joint '%s' has %ld command interfaces. Expected 1.",
                joint.name.c_str(), joint.command_interfaces.size());
            // return hardware_interface::return_type::ERROR;
            return hardware_interface::CallbackReturn::ERROR;

        }
        InterfaceType cmd_interface = str_to_interface(joint.command_interfaces[0].name);
        if (cmd_interface == InterfaceType::INVALID) {
            RCLCPP_FATAL(this->logger_, "Joint '%s' has an invalid command interface: %s",
                joint.name.c_str(), joint.command_interfaces[0].name);
            // return hardware_interface::return_type::ERROR;
            return hardware_interface::CallbackReturn::ERROR;
        }
        cmd->mode = cmd_interface;

        for (auto state_inter : joint.state_interfaces) {
            if (str_to_interface(state_inter.name) == InterfaceType::INVALID) {
                RCLCPP_FATAL(this->logger_, "Joint '%s' has an invalid state interface: %s",
                    joint.name.c_str(), state_inter.name);
                // return hardware_interface::return_type::ERROR;
                return hardware_interface::CallbackReturn::ERROR;
            }
        }

        this->hw_cmd_.push_back(cmd);
        this->hw_status_.push_back(std::make_unique<ros_phoenix::msg::MotorStatus>());
    }

    // return hardware_interface::return_type::OK;
    return hardware_interface::CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface> PhoenixBridge::export_state_interfaces()
{
    std::vector<hardware_interface::StateInterface> state_interfaces;
    for (auto i = 0u; i < this->info_.joints.size(); i++) {
        for (auto state_inter : this->info_.joints[i].state_interfaces) {
            if (state_inter.name == "percent_output")
                state_interfaces.emplace_back(
                    hardware_interface::StateInterface(this->info_.joints[i].name, "percent_output",
                        &(hw_status_[i]->output_percent)));

            else if (state_inter.name == "position")
                state_interfaces.emplace_back(hardware_interface::StateInterface(
                    this->info_.joints[i].name, "position", &(hw_status_[i]->position)));

            else if (state_inter.name == "velocity")
                state_interfaces.emplace_back(hardware_interface::StateInterface(
                    this->info_.joints[i].name, "velocity", &(hw_status_[i]->velocity)));
        }
    }
    return state_interfaces;
}

std::vector<hardware_interface::CommandInterface> PhoenixBridge::export_command_interfaces()
{
    std::vector<hardware_interface::CommandInterface> command_interfaces;
    for (auto i = 0u; i < this->info_.joints.size(); i++) {
        command_interfaces.emplace_back(
            hardware_interface::CommandInterface(this->info_.joints[i].name,
                this->info_.joints[i].command_interfaces[0].name, &(hw_cmd_[i]->value)));
    }
    return command_interfaces;
}

// hardware_interface::return_type PhoenixBridge::start()
hardware_interface::CallbackReturn PhoenixBridge::on_activate(const rclcpp_lifecycle::State & previous_state)
{
    this->node_ = rclcpp::Node::make_shared(this->info_.name);

    for (auto i = 0u; i < this->info_.joints.size(); i++) {
        this->publishers_.push_back(this->node_->create_publisher<ros_phoenix::msg::MotorControl>(
            this->info_.joints[i].name + "/set", 1));

        this->subscribers_.push_back(
            this->node_->create_subscription<ros_phoenix::msg::MotorStatus>(
                this->info_.joints[i].name + "/status", 1,
                [this, i](const ros_phoenix::msg::MotorStatus::SharedPtr status) {
                    *(this->hw_status_[i]) = *status;
                }));
    }

    this->executor_ = rclcpp::executors::SingleThreadedExecutor::make_shared();
    this->executor_->add_node(this->node_);

    this->spin_thread_ = std::thread([this]() { this->executor_->spin(); });

    // return hardware_interface::return_type::OK;
    return hardware_interface::CallbackReturn::SUCCESS;
}

// hardware_interface::return_type PhoenixBridge::stop()
hardware_interface::CallbackReturn PhoenixBridge::on_deactivate(const rclcpp_lifecycle::State & previous_state)

{
    this->executor_->cancel();
    this->spin_thread_.join();

    this->publishers_.clear();
    this->subscribers_.clear();
    this->node_.reset();

    // return hardware_interface::return_type::OK;
    return hardware_interface::CallbackReturn::SUCCESS;
}

// hardware_interface::return_type PhoenixBridge::read()
hardware_interface::return_type PhoenixBridge::read(const rclcpp::Time & time, const rclcpp::Duration & period)

{
    // Do nothing because this->hw_status_ is updated asynchronously in this->spin_thread_
    return hardware_interface::return_type::OK;
}

// hardware_interface::return_type PhoenixBridge::write()
hardware_interface::return_type PhoenixBridge::write(const rclcpp::Time & time, const rclcpp::Duration & period)

{
    for (auto i = 0u; i < this->info_.joints.size(); i++) {
        this->publishers_[i]->publish(*(this->hw_cmd_[i]));
    }
    return hardware_interface::return_type::OK;
}

} // namespace ros_phoenix

#include <pluginlib/class_list_macros.hpp>
PLUGINLIB_EXPORT_CLASS(ros_phoenix::PhoenixBridge, hardware_interface::SystemInterface)