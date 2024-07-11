#include "MainRedisInterface.h"

#include <signal.h>
#include <tinyxml2.h>

#include <filesystem>

#include "controller/RobotControllerConfigParser.h"
#include "simviz/SimVizConfigParser.h"

namespace {

std::string vectorXdToString(const Eigen::VectorXd& vec) {
	std::string str = "\'[";
	for (int i = 0; i < vec.size(); i++) {
		std::cout << vec(i) << std::endl;
		if (vec(i) == -std::numeric_limits<double>::max()) {
			str += "-6.28";
		} else if (vec(i) == std::numeric_limits<double>::max()) {
			str += "6.28";
		} else {
			str += std::to_string(vec(i));
		}
		if (i != vec.size() - 1) {
			str += ",";
		}
	}
	str += "]\'";
	return str;
}

std::string joinVectorOfString(const std::vector<std::string>& elements) {
	std::ostringstream os;
	os << "'[";
	for (auto it = elements.begin(); it != elements.end(); ++it) {
		if (it != elements.begin()) {
			os << ", ";
		}
		os << "\"" << *it << "\"";
	}
	os << "]'";
	return os.str();
}

const std::string CONFIG_FILE_NAME_KEY =
	"sai2::interfaces::main_interface::config_file_name";
const std::string RESET_KEY = "sai2::interfaces::main_interface::reset";

const std::string WEBUI_TEMPLATE_FILE_PATH =
	std::string(UI_FOLDER) + "/web/html/webui_template.html";

std::atomic<bool> controllers_stop_signal = false;
std::atomic<bool> simviz_stop_signal = false;
std::atomic<bool> external_stop_signal = false;
void stop(int i) { ::external_stop_signal = true; }

bool start_simviz = false;
bool simviz_was_stop_by_config_change = false;
bool sim_initialized = false;

}  // namespace

namespace Sai2Interfaces {

MainRedisInterface::MainRedisInterface(const std::string& config_folder_path,
									   const std::string& config_file_name)
	: _config_folder_path(config_folder_path),
	  _config_file_name(config_file_name) {
	// signal handling
	signal(SIGABRT, &stop);
	signal(SIGTERM, &stop);
	signal(SIGINT, &stop);

	// thread for running the interface loop
	std::thread interface_thread(&MainRedisInterface::runInterfaceLoop, this);

	// main loop that runs the graphics and simulation if needed
	while (!::external_stop_signal) {
		if (start_simviz) {
			_simviz_interface =
				make_unique<SimVizRedisInterface>(*_simviz_config.get(), false);
			sim_initialized = true;
			_simviz_interface->run(::simviz_stop_signal);
			if (simviz_was_stop_by_config_change) {
				simviz_was_stop_by_config_change = false;
				start_simviz = false;
			} else {
				::external_stop_signal = true;
			}
		}
		_simviz_interface.reset();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	stopRunningControllers();
	interface_thread.join();
}

bool MainRedisInterface::parseConfig(const std::string& config_file_name) {
	const std::string config_file_path =
		_config_folder_path + "/" + config_file_name;

	tinyxml2::XMLDocument doc;
	if (doc.LoadFile(config_file_path.c_str()) != tinyxml2::XML_SUCCESS) {
		std::cout << "WARNING: Could not load config file: " << config_file_path
				  << std::endl;
		return false;
	}

	_config_file_name = config_file_name;

	_simviz_config = nullptr;
	if (doc.FirstChildElement("simvizConfiguration")) {
		SimVizConfigParser simviz_parser;
		_simviz_config = make_unique<SimVizConfig>(
			simviz_parser.parseConfig(config_file_path));
	}

	_controllers_configs.clear();
	if (doc.FirstChildElement("robotControlConfiguration")) {
		RobotControllerConfigParser controller_parser;
		_controllers_configs = controller_parser.parseConfig(config_file_path);
	}
	return true;
}

std::vector<std::string> getControllerNameAndTasksFromSingleConfig(
	const std::pair<
		std::string,
		std::vector<std::variant<JointTaskConfig, MotionForceTaskConfig>>>
		single_controller_config,
	const Sai2Model::Sai2Model& robot_model) {
	std::string controller_name = single_controller_config.first;
	std::string controller_tasks_names = "[";
	std::string controller_tasks_types = "[";
	std::string controller_tasks_selections = "[";
	for (int i = 0; i < single_controller_config.second.size(); i++) {
		const auto& task = single_controller_config.second.at(i);
		if (std::holds_alternative<JointTaskConfig>(task)) {
			controller_tasks_types += "\"joint_task\"";
			controller_tasks_names +=
				"\"" + get<JointTaskConfig>(task).task_name + "\"";
			auto controlled_joint_names =
				get<JointTaskConfig>(task).controlled_joint_names;
			if (!controlled_joint_names.empty()) {
				controller_tasks_selections +=
					"[" + to_string(robot_model.jointIndex(
							  controlled_joint_names[0]));
				if (controlled_joint_names.size() > 1) {
					for (int j = 1; j < controlled_joint_names.size(); j++) {
						controller_tasks_selections +=
							", " + to_string(robot_model.jointIndex(
									   controlled_joint_names[j]));
					}
				}
				controller_tasks_selections += "]";
			} else {
				controller_tasks_selections += "[]";
			}
		} else if (std::holds_alternative<MotionForceTaskConfig>(task)) {
			controller_tasks_types += "\"motion_force_task\"";
			controller_tasks_names +=
				"\"" + get<MotionForceTaskConfig>(task).task_name + "\"";
			controller_tasks_selections += "[]";
		} else {
			std::cerr << "Error: Unknown task type in generating ui html.\n";
		}
		if (i != single_controller_config.second.size() - 1) {
			controller_tasks_names += ", ";
			controller_tasks_types += ", ";
			controller_tasks_selections += ", ";
		}
	}
	controller_tasks_names += "]";
	controller_tasks_types += "]";
	controller_tasks_selections += "]";
	return std::vector<std::string>{controller_name, controller_tasks_names,
									controller_tasks_types,
									controller_tasks_selections};
}

std::vector<std::string> generateControllerNamesAndTasksForUI(
	const RobotControllerConfig& config,
	const Sai2Model::Sai2Model& robot_model) {
	std::string controller_names = "[";
	std::string controller_tasks_names = "[";
	std::string controller_tasks_types = "[";
	std::string controller_task_selections = "[";

	for (auto it = config.controllers_configs.begin();
		 it != config.controllers_configs.end(); ++it) {
		const auto& pair = *it;
		const auto& controller_name = pair.first;

		auto controller_ui_specs =
			getControllerNameAndTasksFromSingleConfig(pair, robot_model);
		if (it != config.controllers_configs.begin()) {
			controller_names += ", ";
			controller_tasks_names += ", ";
			controller_tasks_types += ", ";
			controller_task_selections += ", ";
		}
		controller_names += "\"" + controller_ui_specs[0] + "\"";
		controller_tasks_names += controller_ui_specs[1];
		controller_tasks_types += controller_ui_specs[2];
		controller_task_selections += controller_ui_specs[3];
	}

	controller_names += "]";
	controller_tasks_names += "]";
	controller_tasks_types += "]";
	controller_task_selections += "]";
	return std::vector<std::string>{controller_names, controller_tasks_names,
									controller_tasks_types,
									controller_task_selections};
}

void MainRedisInterface::generateUiFile() {
	std::ifstream templateHtml(WEBUI_TEMPLATE_FILE_PATH);
	if (!templateHtml) {
		std::cerr << "Error: Unable to open template HTML file.\n";
		return;
	}

	// Read the content of the original HTML file
	std::string htmlContent((std::istreambuf_iterator<char>(templateHtml)),
							(std::istreambuf_iterator<char>()));

	// Close the original file
	templateHtml.close();

	std::string additionalContent;
	if (_controllers_configs.size() == 0 && !_simviz_interface) {
		// do nothing
	} else if (_controllers_configs.size() == 1 && !_simviz_interface) {
		Sai2Model::Sai2Model robot_model = Sai2Model::Sai2Model(
			_controllers_configs[0].robot_model_file, false);

		std::vector<std::string> controller_names_and_tasks =
			generateControllerNamesAndTasksForUI(_controllers_configs[0],
												 robot_model);
		additionalContent =
			"<div class='row mx-3'>\n<sai2-interfaces-robot-controller "
			"robotName='" +
			_controllers_configs[0].robot_name + "'\nredisPrefix='" +
			_controllers_configs[0].redis_prefix + "'\ncontrollerNames='" +
			controller_names_and_tasks[0] + "'\ncontrollerTaskNames='" +
			controller_names_and_tasks[1] + "'\ncontrollerTaskTypes='" +
			controller_names_and_tasks[2] + "'\ncontrollerTaskSelections='" +
			controller_names_and_tasks[3] + "'";

		// joint limits in interface
		additionalContent += "\nlowerJointLimits=" +
							 vectorXdToString(robot_model.jointLimitsPositionLower());
		additionalContent += "\nupperJointLimits=" +
							 vectorXdToString(robot_model.jointLimitsPositionUpper());

		// joint names in interface
		additionalContent +=
			"\njointNames=" + joinVectorOfString(robot_model.jointNames());

		// interface config for positions and force slider limits
		additionalContent +=
			"\nminGoalPosition='" +
			_controllers_configs[0].interface_config.min_goal_position + "'";
		additionalContent +=
			"\nmaxGoalPosition='" +
			_controllers_configs[0].interface_config.max_goal_position + "'";
		additionalContent +=
			"\nminDesiredForce='" +
			_controllers_configs[0].interface_config.min_desired_force + "'";
		additionalContent +=
			"\nmaxDesiredForce='" +
			_controllers_configs[0].interface_config.max_desired_force + "'";
		additionalContent +=
			"\nminDesiredMoment='" +
			_controllers_configs[0].interface_config.min_desired_moment + "'";
		additionalContent +=
			"\nmaxDesiredMoment='" +
			_controllers_configs[0].interface_config.max_desired_moment + "'";
		additionalContent += " />\n</div>\n";
	} else {
		additionalContent += "<div class='row mx-3'>\n";
		// get random number for tabs name
		std::string random_number = std::to_string(rand());

		additionalContent += "<sai2-interfaces-tabs name='Robot_names" +
							 random_number + "' color='#b30000'>\n";

		for (const auto& config : _controllers_configs) {
			Sai2Model::Sai2Model robot_model =
				Sai2Model::Sai2Model(config.robot_model_file, false);

			additionalContent += "<sai2-interfaces-tab-content name='" +
								 config.robot_name + "'>\n";

			std::vector<std::string> controller_names_and_tasks =
				generateControllerNamesAndTasksForUI(config, robot_model);
			additionalContent +=
				"<div class='row my-3'>\n<sai2-interfaces-robot-controller "
				"robotName='" +
				config.robot_name + "'\nredisPrefix='" + config.redis_prefix +
				"'\ncontrollerNames='" + controller_names_and_tasks[0] +
				"'\ncontrollerTaskNames='" + controller_names_and_tasks[1] +
				"'\ncontrollerTaskTypes='" + controller_names_and_tasks[2] +
				"'\ncontrollerTaskSelections='" +
				controller_names_and_tasks[3] + "'";

			// joint limits in interface
			additionalContent +=
				"\nlowerJointLimits=" +
				vectorXdToString(robot_model.jointLimitsPositionLower());
			additionalContent +=
				"\nupperJointLimits=" +
				vectorXdToString(robot_model.jointLimitsPositionUpper());

			// joint names in interface
			additionalContent +=
				"\njointNames=" + joinVectorOfString(robot_model.jointNames());

			// interface config for positions and force slider limits
			additionalContent += "\nminGoalPosition='" +
								 config.interface_config.min_goal_position +
								 "'";
			additionalContent += "\nmaxGoalPosition='" +
								 config.interface_config.max_goal_position +
								 "'";
			additionalContent += "\nminDesiredForce='" +
								 config.interface_config.min_desired_force +
								 "'";
			additionalContent += "\nmaxDesiredForce='" +
								 config.interface_config.max_desired_force +
								 "'";
			additionalContent += "\nminDesiredMoment='" +
								 config.interface_config.min_desired_moment +
								 "'";
			additionalContent += "\nmaxDesiredMoment='" +
								 config.interface_config.max_desired_moment +
								 "'";

			additionalContent += " />\n</div>\n";

			additionalContent += "</sai2-interfaces-tab-content>\n";
		}

		if (_simviz_interface) {
			additionalContent +=
				"<sai2-interfaces-tab-content name='Simviz'>\n";
			additionalContent += "<div class='row my-3'>\n";

			std::string model_names = "\'[";
			std::string model_types = "\'[";

			for (const auto& name : _simviz_interface->getRobotNames()) {
				model_names += "\"" + name + "\",";
				model_types += "\"robot\",";
			}
			for (const auto& name : _simviz_interface->getObjectNames()) {
				model_names += "\"" + name + "\",";
				model_types += "\"object\",";
			}
			// remove the last comma
			model_names.pop_back();
			model_types.pop_back();

			model_names += "]\'";
			model_types += "]\'";

			additionalContent += "<sai2-interfaces-simviz\nredisPrefix='" +
								 _simviz_config->redis_prefix +
								 "'\nmodelNames=" + model_names +
								 "\nmodelTypes=" + model_types + " />\n";

			additionalContent += "</div>\n";
			additionalContent += "</sai2-interfaces-tab-content>\n";
		}

		additionalContent += "</sai2-interfaces-tabs>\n";
		additionalContent += "</div>\n";
	}

	// Add content before the </body> tag
	size_t bodyPosition = htmlContent.find("</body>");
	if (bodyPosition != std::string::npos) {
		htmlContent.insert(bodyPosition, additionalContent);
	} else {
		std::cerr << "Error: </body> tag not found in template HTML file.\n";
		return;
	}

	// Write the modified content to a new file
	const std::string webui_generated_dir =
		_config_folder_path + "/webui_generated_file";
	std::filesystem::create_directories(webui_generated_dir);
	std::ofstream modifiedFile(webui_generated_dir + "/webui.html");
	if (!modifiedFile) {
		std::cerr << "Error: Unable to create modified HTML file.\n";
	}

	modifiedFile << htmlContent;

	// Close the modified file
	modifiedFile.close();

	std::cout << "Webui HTML file has been created successfully.\n";
}

void MainRedisInterface::runInterfaceLoop() {
	Sai2Common::RedisClient redis_client;
	redis_client.connect();

	redis_client.set(CONFIG_FILE_NAME_KEY, _config_file_name);
	redis_client.setBool(RESET_KEY, true);

	while (!::external_stop_signal) {
		std::string new_config_file_name =
			redis_client.get(CONFIG_FILE_NAME_KEY);

		if (_config_file_name != new_config_file_name ||
			redis_client.getBool(RESET_KEY)) {
			bool succes = parseConfig(new_config_file_name);
			if (succes) {
				reset();
			}
			redis_client.setBool(RESET_KEY, false);
			redis_client.set(CONFIG_FILE_NAME_KEY, _config_file_name);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	::controllers_stop_signal = true;
	::simviz_stop_signal = true;
}

void MainRedisInterface::reset() {
	// first stop controllers
	stopRunningControllers();

	// next handle simulation
	if (_simviz_config && !_simviz_interface) {
		start_simviz = true;
		sim_initialized = false;
		while (!sim_initialized) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	} else if (!_simviz_config && _simviz_interface) {
		::simviz_stop_signal = true;
		simviz_was_stop_by_config_change = true;
	} else if (_simviz_config && _simviz_interface) {
		_simviz_interface->reset(*_simviz_config);
		while (!_simviz_interface->isResetComplete()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	// generate new ui file
	generateUiFile();

	// finally start new controllers
	startNewControllers();
}

void MainRedisInterface::startNewControllers() {
	::controllers_stop_signal = false;
	_controllers_threads.clear();
	for (const auto& config : _controllers_configs) {
		_controllers_interfaces[config.robot_name] =
			make_unique<RobotControllerRedisInterface>(config, false);
		_controllers_threads.push_back(std::thread([&]() {
			_controllers_interfaces.at(config.robot_name)
				->run(::controllers_stop_signal);
		}));
	}
}

void MainRedisInterface::stopRunningControllers() {
	::controllers_stop_signal = true;
	for (auto& thread : _controllers_threads) {
		thread.join();
	}
	_controllers_threads.clear();
	_controllers_interfaces.clear();
}

}  // namespace Sai2Interfaces