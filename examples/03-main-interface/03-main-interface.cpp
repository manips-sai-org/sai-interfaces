#include "MainRedisInterface.h"

int main(int argc, char** argv) {
	// add world file folder to search path for parser
	Sai2Model::URDF_FOLDERS["WORLD_FILES_FOLDER"] =
		std::string(EXAMPLE_FOLDER_PATH) + "/world_files";

	// define the config folder. Only config files in that folder can be used by
	// this application
	std::string config_folder_path =
		std::string(EXAMPLE_FOLDER_PATH) + "/03-main-interface";

	// initial config file (optionnal)
	std::string config_file = "config_panda.xml";

	Sai2Interfaces::MainRedisInterface main_interface(config_folder_path,
													  config_file);

	return 0;
}