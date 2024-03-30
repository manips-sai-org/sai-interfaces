#ifndef SAI2_INTERFACES_ROBOT_CONTROLLER_REDIS_INTERFACE_H
#define SAI2_INTERFACES_ROBOT_CONTROLLER_REDIS_INTERFACE_H

#include <string>

#include "RobotControllerConfig.h"
#include "Sai2Common.h"
#include "Sai2Model.h"
#include "Sai2Primitives.h"

namespace Sai2Interfaces {

class RobotControllerRedisInterface {
public:
	RobotControllerRedisInterface(const RobotControllerConfig& config,
								  const bool setup_signal_handler = true);
	~RobotControllerRedisInterface() = default;

	void run(const std::atomic<bool>& user_stop_signal = false);

private:
	enum LoggingState {
		OFF,
		START,
		ON,
		STOP,
	};
	struct JointTaskInput {
		Eigen::VectorXd goal_position;
		Eigen::VectorXd goal_velocity;
		Eigen::VectorXd goal_acceleration;

		JointTaskInput() {
			goal_position.setZero(1);
			goal_velocity.setZero(1);
			goal_acceleration.setZero(1);
		};
		JointTaskInput(const int& dofs) {
			goal_position.setZero(dofs);
			goal_velocity.setZero(dofs);
			goal_acceleration.setZero(dofs);
		}

		void setFromTask(
			const std::shared_ptr<Sai2Primitives::JointTask>& task) {
			goal_position = task->getGoalPosition();
			goal_velocity = task->getGoalVelocity();
			goal_acceleration = task->getGoalAcceleration();
		}
	};

	struct JointTaskMonitoringData {
		Eigen::VectorXd current_position;
		Eigen::VectorXd current_velocity;
		Eigen::VectorXd desired_position;
		Eigen::VectorXd desired_velocity;
		Eigen::VectorXd desired_acceleration;

		JointTaskMonitoringData() {
			current_position.setZero(1);
			current_velocity.setZero(1);
			desired_position.setZero(1);
			desired_velocity.setZero(1);
			desired_acceleration.setZero(1);
		}
		JointTaskMonitoringData(const int& dofs) {
			current_position.setZero(dofs);
			current_velocity.setZero(dofs);
			desired_position.setZero(dofs);
			desired_velocity.setZero(dofs);
			desired_acceleration.setZero(dofs);
		}

		void setFromTask(
			const std::shared_ptr<Sai2Primitives::JointTask>& task) {
			current_position = task->getCurrentPosition();
			current_velocity = task->getCurrentVelocity();
			desired_position = task->getDesiredPosition();
			desired_velocity = task->getDesiredVelocity();
			desired_acceleration = task->getDesiredAcceleration();
		}
	};

	struct MotionForceTaskInput {
		Eigen::Vector3d goal_position;
		Eigen::Vector3d goal_linear_velocity;
		Eigen::Vector3d goal_linear_acceleration;
		Eigen::Matrix3d goal_orientation;
		Eigen::Vector3d goal_angular_velocity;
		Eigen::Vector3d goal_angular_acceleration;
		Eigen::Vector3d desired_force;
		Eigen::Vector3d desired_moment;
		Eigen::Vector3d sensed_force_sensor_frame;
		Eigen::Vector3d sensed_moment_sensor_frame;

		MotionForceTaskInput() {
			goal_position.setZero();
			goal_linear_velocity.setZero();
			goal_linear_acceleration.setZero();
			goal_orientation.setIdentity();
			goal_angular_velocity.setZero();
			goal_angular_acceleration.setZero();
			desired_force.setZero();
			desired_moment.setZero();
			sensed_force_sensor_frame.setZero();
			sensed_moment_sensor_frame.setZero();
		}

		void setFromTask(
			const std::shared_ptr<Sai2Primitives::MotionForceTask>& task) {
			goal_position = task->getGoalPosition();
			goal_linear_velocity = task->getGoalLinearVelocity();
			goal_linear_acceleration = task->getGoalLinearAcceleration();
			goal_orientation = task->getGoalOrientation();
			goal_angular_velocity = task->getGoalAngularVelocity();
			goal_angular_acceleration = task->getGoalAngularAcceleration();
			desired_force = task->getGoalForce();
			desired_moment = task->getGoalMoment();
			sensed_force_sensor_frame = task->getSensedForceSensor();
			sensed_moment_sensor_frame = task->getSensedMomentSensor();
		}
	};

	struct MotionForceTaskMonitoringData {
		Eigen::Vector3d current_position;
		Eigen::Vector3d current_linear_velocity;
		Eigen::Matrix3d current_orientation;
		Eigen::Vector3d current_angular_velocity;
		Eigen::Vector3d desired_position;
		Eigen::Vector3d desired_linear_velocity;
		Eigen::Vector3d desired_linear_acceleration;
		Eigen::Matrix3d desired_orientation;
		Eigen::Vector3d desired_angular_velocity;
		Eigen::Vector3d desired_angular_acceleration;
		Eigen::Vector3d sensed_force_world_frame;
		Eigen::Vector3d sensed_moment_world_frame;

		MotionForceTaskMonitoringData() {
			current_position.setZero();
			current_linear_velocity.setZero();
			current_orientation.setIdentity();
			current_angular_velocity.setZero();
			desired_position.setZero();
			desired_linear_velocity.setZero();
			desired_linear_acceleration.setZero();
			desired_orientation.setIdentity();
			desired_angular_velocity.setZero();
			desired_angular_acceleration.setZero();
			sensed_force_world_frame.setZero();
			sensed_moment_world_frame.setZero();
		}

		void setFromTask(
			const std::shared_ptr<Sai2Primitives::MotionForceTask>& task) {
			current_position = task->getCurrentPosition();
			current_linear_velocity = task->getCurrentLinearVelocity();
			current_orientation = task->getCurrentOrientation();
			current_angular_velocity = task->getCurrentAngularVelocity();
			desired_position = task->getDesiredPosition();
			desired_linear_velocity = task->getDesiredLinearVelocity();
			desired_linear_acceleration = task->getDesiredLinearAcceleration();
			desired_orientation = task->getDesiredOrientation();
			desired_angular_velocity = task->getDesiredAngularVelocity();
			desired_angular_acceleration =
				task->getDesiredAngularAcceleration();
			sensed_force_world_frame = task->getSensedForceControlWorldFrame();
			sensed_moment_world_frame =
				task->getSensedMomentControlWorldFrame();
		}
	};

	typedef std::variant<JointTaskInput, MotionForceTaskInput> TaskInputVariant;
	typedef std::variant<JointTaskMonitoringData, MotionForceTaskMonitoringData>
		TaskMonitoringDataVariant;

	void initialize();
	void initializeRedisTasksIO();

	void switchController(const std::string& controller_name);
	void processInputs();

	RobotControllerConfig _config;

	std::shared_ptr<Sai2Model::Sai2Model> _robot_model;
	std::map<std::string, std::unique_ptr<Sai2Primitives::RobotController>>
		_robot_controllers;
	std::string _active_controller_name;

	Sai2Common::RedisClient _redis_client;

	Eigen::VectorXd _robot_q;
	Eigen::VectorXd _robot_dq;
	Eigen::VectorXd _robot_command_torques;
	Eigen::MatrixXd _robot_M;

	std::map<std::string, std::map<std::string, TaskInputVariant>>
		_controller_inputs;
	std::map<std::string, std::map<std::string, TaskMonitoringDataVariant>>
		_controller_task_monitoring_data;

	std::map<std::string,
			 std::map<std::string, std::unique_ptr<Sai2Common::Logger>>>
		_task_loggers;
	std::unique_ptr<Sai2Common::Logger> _robot_logger;
	std::map<std::string, bool> _is_active_controller;
	bool _logging_on;
	LoggingState _logging_state;
};

}  // namespace Sai2Interfaces

#endif	// SAI2_INTERFACES_ROBOT_CONTROLLER_REDIS_INTERFACE_H