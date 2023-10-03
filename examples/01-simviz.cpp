#include "simviz/SimVizRedisInterface.h"
#include "simviz/SimVizConfigParser.h"

int main(int argc, char** argv) {

    std::string config_file = "resources/simviz_config_2.xml";
    // Sai2Interfaces::SimVizConfigParser parser;
    // Sai2Interfaces::SimVizConfig config = parser.parseConfig(config_file);

    // std::string world_file = "resources/world3.urdf";
    Sai2Interfaces::SimVizRedisInterface simviz(config_file);
    simviz.run();
    return 0;
}