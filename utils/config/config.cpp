#include "utils/config/config.h"

#include <X11/Xutil.h>

#include <iostream>

namespace Utils {
  Config parseConfig(Display *display, std::string configFile) {
    Config config;

    // Parse YAML
    try {
      YAML::Node yamlConfig = YAML::LoadFile(configFile);
      auto launchData = yamlConfig["launchData"];
      if (!launchData) {
        std::cout << "Fatal Error: Must specify launch data." << std::endl;
        exit(1);
      }
      for (int i = 0; i < launchData.size(); i++) {
        if (!launchData[i]["key"]) {
          std::cout <<
            "Fatal Error: Must specify \"key\" for every item in \"launchData\"."
            << std::endl;
          exit(1);
        }
        if (!launchData[i]["binPath"]) {
          std::cout <<
            "Fatal Error: Must specify \"binPath\" "
            << "for every item in \"launchData\"."
            << std::endl;
          exit(1);
        }

        std::string key = launchData[i]["key"].as<std::string>();
        KeySym keySym = XStringToKeysym(key.c_str());
        if (keySym == NoSymbol) {
          std::cout << "Error: The specified key \"" << key <<
            "\" is invalid. Please specify a key in the proper format." <<
            std::endl;
          continue;
        }
        int keyCode = XKeysymToKeycode(display, keySym);
        std::string binPath = launchData[i]["binPath"].as<std::string>();
        std::string toggleBinPath;
        if (launchData[i]["toggleBinPath"]) {
          toggleBinPath = launchData[i]["toggleBinPath"].as<std::string>();
        } else {
          toggleBinPath = binPath;
        }
        auto argsNode = launchData[i]["args"];
        std::vector<char*> args = {};
        for (int j = 0; j < argsNode.size(); j++) {
          argsNode.push_back(argsNode[j].as<std::string>().c_str());
        }
        args.push_back(nullptr);
        config.launchData.push_back(ApplicationLaunchData {
          key, keyCode, binPath, toggleBinPath, args
        });
      }
    }
    catch (...) {
      std::cout << "Fatal Error: Failed to parse YAML configuration file."
        << std::endl;
      exit(1);
    }

    // Construct key->bin mapping
    for (auto launchData : config.launchData) {
      config.keyDataMapping[launchData.keyCode] = launchData;
    }

    return config;
  }
}  // namespace Utils
