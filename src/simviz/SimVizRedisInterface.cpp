#include "SimVizRedisInterface.h"

#include <signal.h>

#include <filesystem>

namespace Sai2Interfaces {

namespace {
bool external_stop_signal = false;
void stop(int i) { external_stop_signal = true; }

const std::string sim_param_group_name = "simviz_redis_sim_param_group_name";

// redis keys
const std::string group_name = "simviz_redis_group_name";

}  // namespace

SimVizRedisInterface::SimVizRedisInterface(const SimVizConfig& config,
										   const bool setup_signal_handler)
	: _config(config),
	  _new_config(config),
	  _pause(false),
	  _reset(false),
	  _enable_grav_comp(true),
	  _logging_on(false),
	  _reset_complete(false),
	  _logging_state(LoggingState::OFF) {
	_graphics = std::make_unique<Sai2Graphics::Sai2Graphics>(
		_config.world_file, "sai2 world", false);
	_simulation = std::make_unique<Sai2Simulation::Sai2Simulation>(
		_config.world_file, false);

	_redis_client.connect();
	_redis_client.createNewSendGroup(group_name);
	_redis_client.createNewReceiveGroup(group_name);
	initializeRedisDatabase();
	resetInternal();

	if (setup_signal_handler) {
		signal(SIGABRT, &stop);
		signal(SIGTERM, &stop);
		signal(SIGINT, &stop);
	}
}

void SimVizRedisInterface::reset(const SimVizConfig& config) {
	_reset_complete = false;
	_new_config = config;
	_reset = true;
}

void SimVizRedisInterface::resetInternal() {
	_simulation->setTimestep(_config.timestep);
	_simulation->setCollisionRestitution(_config.collision_restitution);
	_simulation->setCoeffFrictionStatic(_config.friction_coefficient);

	_redis_client.setBool(
		_config.redis_prefix + "::simviz::gravity_comp_enabled",
		_config.enable_gravity_compensation);
	_simulation->enableGravityCompensation(_config.enable_gravity_compensation);

	_robot_ui_torques.clear();
	_object_ui_torques.clear();
	_redis_client.deleteSendGroup(group_name);
	_redis_client.deleteReceiveGroup(group_name);
	_redis_client.createNewSendGroup(group_name);
	_redis_client.createNewReceiveGroup(group_name);

	_robot_control_torques.clear();
	_robot_q.clear();
	_robot_dq.clear();
	_object_pose.clear();
	_object_vel.clear();
	_force_sensor_data.clear();

	_loggers.clear();
	if (!std::filesystem::exists(_config.logger_config.folder_name)) {
		std::filesystem::create_directories(_config.logger_config.folder_name);
	}

	_logging_on = _config.logger_config.start_with_logger_on;
	_redis_client.setBool(_config.redis_prefix + "::simviz::logging_on",
						  _logging_on);
	_logging_state = _logging_on ? LoggingState::START : LoggingState::OFF;

	for (auto& robot_name : _simulation->getRobotNames()) {
		// dof and joint positions and velocities initially
		const int robot_dof = _simulation->dof(robot_name);
		_robot_q[robot_name] = _simulation->getJointPositions(robot_name);
		_robot_dq[robot_name] = _simulation->getJointVelocities(robot_name);

		// logger for all modes
		_loggers[robot_name] = std::make_unique<Sai2Common::Logger>(
			_config.logger_config.folder_name + "/" + robot_name,
			_config.logger_config.add_timestamp_to_filename);
		_loggers.at(robot_name)->addToLog(_robot_q.at(robot_name), "q");
		_loggers.at(robot_name)->addToLog(_robot_dq.at(robot_name), "dq");

		// setup if there is simulation
		if (_config.mode != SimVizMode::VIZ_ONLY) {
			if (_config.enable_joint_limits) {
				_simulation->enableJointLimits(robot_name);
			} else {
				_simulation->disableJointLimits(robot_name);
			}

			_graphics->addUIForceInteraction(robot_name);
			_robot_ui_torques[robot_name] = VectorXd::Zero(robot_dof);
			_robot_control_torques[robot_name] = VectorXd::Zero(robot_dof);

			// redis
			_redis_client.addToReceiveGroup(
				_config.redis_prefix + "::robot_command_torques::" + robot_name,
				_robot_control_torques.at(robot_name), group_name);
			_redis_client.addToSendGroup(
				_config.redis_prefix + "::robot_q::" + robot_name,
				_robot_q.at(robot_name), group_name);
			_redis_client.addToSendGroup(
				_config.redis_prefix + "::robot_dq::" + robot_name,
				_robot_dq.at(robot_name), group_name);

			// logger
			_loggers.at(robot_name)
				->addToLog(_robot_control_torques.at(robot_name),
						   "command_torques");
			_loggers.at(robot_name)
				->addToLog(_robot_ui_torques.at(robot_name), "ui_torques");
		} else {  // setup for viz only
			_redis_client.addToReceiveGroup(
				_config.redis_prefix + "::robot_q::" + robot_name,
				_robot_q.at(robot_name), group_name);
			_redis_client.addToReceiveGroup(
				_config.redis_prefix + "::robot_dq::" + robot_name,
				_robot_dq.at(robot_name), group_name);
		}
	}

	for (auto& object_name : _simulation->getObjectNames()) {
		// initial object pose and velocity
		_object_pose[object_name] =
			_simulation->getObjectPose(object_name).matrix();
		_object_vel[object_name] = _simulation->getObjectVelocity(object_name);

		// logger for all modes
		_loggers[object_name] = std::make_unique<Sai2Common::Logger>(
			_config.logger_config.folder_name + "/" + object_name,
			_config.logger_config.add_timestamp_to_filename);
		_loggers.at(object_name)
			->addToLog(_object_pose.at(object_name), "pose");
		_loggers.at(object_name)
			->addToLog(_object_vel.at(object_name), "velocity");

		// setup if there is simulation
		if (_config.mode != SimVizMode::VIZ_ONLY) {
			_redis_client.addToSendGroup(
				_config.redis_prefix + "::simviz::obj_pose::" + object_name,
				_object_pose.at(object_name), group_name);
			_redis_client.addToSendGroup(
				_config.redis_prefix + "::simiz::obj_velocity::" + object_name,
				_object_vel.at(object_name), group_name);

			_graphics->addUIForceInteraction(object_name);
			_object_ui_torques[object_name] = Eigen::VectorXd::Zero(6);

			_loggers.at(object_name)
				->addToLog(_object_ui_torques.at(object_name), "ui_torques");
		} else {  // setup for viz only
			_redis_client.addToReceiveGroup(
				_config.redis_prefix + "::simviz::obj_pose::" + object_name,
				_object_pose.at(object_name), group_name);
			_redis_client.addToReceiveGroup(
				_config.redis_prefix + "::simviz::obj_velocity::" + object_name,
				_object_vel.at(object_name), group_name);
		}
	}

	// force sensors only if there is simulation
	if (_config.mode != SimVizMode::VIZ_ONLY) {
		for (const auto& force_sensor_config : _config.force_sensors) {
			_simulation->addSimulatedForceSensor(
				force_sensor_config.robot_name, force_sensor_config.link_name,
				force_sensor_config.transform_in_link,
				force_sensor_config.cutoff_frequency);
		}

		_force_sensor_data = _simulation->getAllForceSensorData();
		for (auto& force_sensor_data : _force_sensor_data) {
			_graphics->addForceSensorDisplay(force_sensor_data);
			_redis_client.addToSendGroup(
				_config.redis_prefix +
					"::force_sensor::" + force_sensor_data.robot_name +
					"::" + force_sensor_data.link_name + "::force",
				force_sensor_data.force_local_frame, group_name);
			_redis_client.addToSendGroup(
				_config.redis_prefix +
					"::force_sensor::" + force_sensor_data.robot_name +
					"::" + force_sensor_data.link_name + "::moment",
				force_sensor_data.moment_local_frame, group_name);

			_loggers.at(force_sensor_data.robot_name)
				->addToLog(force_sensor_data.force_local_frame,
						   force_sensor_data.link_name + "_sensed_force");
			_loggers.at(force_sensor_data.robot_name)
				->addToLog(force_sensor_data.moment_local_frame,
						   force_sensor_data.link_name + "_sensed_moment");
		}
	}

	_reset_complete = true;
}

void SimVizRedisInterface::initializeRedisDatabase() {
	_redis_client.createNewReceiveGroup(sim_param_group_name);
	_redis_client.addToReceiveGroup(_config.redis_prefix + "::simviz::pause",
									_pause, sim_param_group_name);
	_redis_client.addToReceiveGroup(
		_config.redis_prefix + "::simviz::gravity_comp_enabled",
		_enable_grav_comp, sim_param_group_name);
	_redis_client.addToReceiveGroup(
		_config.redis_prefix + "::simviz::logging_on", _logging_on,
		sim_param_group_name);
}

void SimVizRedisInterface::run(const std::atomic<bool>& user_stop_signal) {
	std::thread sim_thread;
	sim_thread = std::thread(&SimVizRedisInterface::simLoopRun, this,
							 std::ref(user_stop_signal));
	if (_config.mode != SimVizMode::SIM_ONLY) {
		vizLoopRun(user_stop_signal);
	}
	sim_thread.join();
}

void SimVizRedisInterface::vizLoopRun(
	const std::atomic<bool>& user_stop_signal) {
	Sai2Common::LoopTimer timer(30.0);

	while (!user_stop_signal && !external_stop_signal &&
		   _graphics->isWindowOpen()) {
		this_thread::sleep_for(chrono::microseconds(50));
		timer.waitForNextLoop();

		std::lock_guard<std::mutex> lock(_mutex_parametrization);
		for (auto& robot_name : _graphics->getRobotNames()) {
			_graphics->updateRobotGraphics(robot_name, _robot_q.at(robot_name),
										   _robot_dq.at(robot_name));
		}
		for (auto& object_name : _graphics->getObjectNames()) {
			_graphics->updateObjectGraphics(
				object_name, Eigen::Affine3d(_object_pose.at(object_name)),
				_object_vel.at(object_name));
		}
		if (_config.mode != SimVizMode::VIZ_ONLY) {
			for (auto& force_sensor_data : _force_sensor_data) {
				_graphics->updateDisplayedForceSensor(force_sensor_data);
			}
		}
		_graphics->renderGraphicsWorld();
		if (_config.mode != SimVizMode::VIZ_ONLY) {
			for (auto& robot_name : _graphics->getRobotNames()) {
				std::lock_guard<std::mutex> lock(_mutex_torques);
				_robot_ui_torques.at(robot_name) =
					_graphics->getUITorques(robot_name);
			}
			for (auto& object_name : _graphics->getObjectNames()) {
				std::lock_guard<std::mutex> lock(_mutex_torques);
				_object_ui_torques.at(object_name) =
					_graphics->getUITorques(object_name);
			}
		}
	}
	external_stop_signal = true;
}

void SimVizRedisInterface::simLoopRun(
	const std::atomic<bool>& user_stop_signal) {
	Sai2Common::LoopTimer timer(_config.speedup_factor /
								_simulation->timestep());
	timer.setTimerName("Simviz Redis Interface Loop Timer");

	while (!user_stop_signal && !external_stop_signal) {
		_redis_client.receiveAllFromGroup(sim_param_group_name);
		processSimParametrization(timer);

		if (_config.mode == SimVizMode::VIZ_ONLY) {
			timer.waitForNextLoop();
			_redis_client.receiveAllFromGroup(group_name);
			continue;
		}

		if (_pause) {
			timer.stop();
			_simulation->pause();
		} else {
			if (_simulation->isPaused()) {
				_simulation->unpause();
				timer.reinitializeTimer();
			}
			timer.waitForNextLoop();
			_redis_client.receiveAllFromGroup(group_name);
			for (auto& robot_name : _simulation->getRobotNames()) {
				std::lock_guard<std::mutex> lock(_mutex_torques);
				_simulation->setJointTorques(
					robot_name, _robot_ui_torques.at(robot_name) +
									_robot_control_torques.at(robot_name));
			}
			for (auto& object_name : _simulation->getObjectNames()) {
				std::lock_guard<std::mutex> lock(_mutex_torques);
				_simulation->setObjectForceTorque(
					object_name, _object_ui_torques.at(object_name));
			}

			_simulation->integrate();

			for (auto& robot_name : _simulation->getRobotNames()) {
				_robot_q.at(robot_name) =
					_simulation->getJointPositions(robot_name);
				_robot_dq.at(robot_name) =
					_simulation->getJointVelocities(robot_name);
			}
			for (auto& object_name : _simulation->getObjectNames()) {
				_object_pose.at(object_name) =
					_simulation->getObjectPose(object_name).matrix();
				_object_vel.at(object_name) =
					_simulation->getObjectVelocity(object_name);
			}
			_force_sensor_data = _simulation->getAllForceSensorData();
			_redis_client.sendAllFromGroup(group_name);
		}
	}
	timer.stop();

	for (auto& logger : _loggers) {
		logger.second->stop();
	}

	if (_config.mode != SimVizMode::VIZ_ONLY) {
		timer.printInfoPostRun();
	}
}

void SimVizRedisInterface::processSimParametrization(
	Sai2Common::LoopTimer& timer) {
	if (_reset) {
		std::lock_guard<mutex> lock(_mutex_parametrization);
		_config = _new_config;
		_simulation->resetWorld(_config.world_file);
		_graphics->resetWorld(_config.world_file);
		resetInternal();
		timer.resetLoopFrequency(_config.speedup_factor /
								 _simulation->timestep());
		timer.reinitializeTimer(1e6);
		_reset = false;
	}

	if (_enable_grav_comp != _simulation->isGravityCompensationEnabled()) {
		_simulation->enableGravityCompensation(_enable_grav_comp);
	}

	switch (_logging_state) {
		case LoggingState::OFF:
			if (_logging_on) {
				_logging_state = LoggingState::START;
			}
			break;
		case LoggingState::START:
			for (auto& logger : _loggers) {
				logger.second->start(_config.logger_config.frequency);
			}
			_logging_state = LoggingState::ON;
			break;

		case LoggingState::ON:
			if (!_logging_on) {
				_logging_state = LoggingState::STOP;
			}
			break;

		case LoggingState::STOP:
			for (auto& logger : _loggers) {
				logger.second->stop();
			}
			_logging_state = LoggingState::OFF;
			break;

		default:
			break;
	}
}

}  // namespace Sai2Interfaces