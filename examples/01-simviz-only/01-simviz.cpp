#include "simviz/SimVizRedisInterface.h"
#include "simviz/SimVizConfigParser.h"

int main(int argc, char** argv) {
    std::string config_file = "resources/simviz_config_panda.xml";
    Sai2Interfaces::SimVizRedisInterface simviz(config_file);
    simviz.run();
    return 0;
}