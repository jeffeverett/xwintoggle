#pragma once

#include <yaml-cpp/yaml.h>
#include <X11/Xlib.h>

#include <string>
#include <vector>
#include <unordered_map>

typedef struct ApplicationLaunchData {
  std::string key;
  int keyCode;
  std::string binPath;
  std::string toggleBinPath;
  std::vector<char*> args;
} ApplicationLaunchData;

typedef struct Config {
  std::vector<ApplicationLaunchData> launchData;
  std::unordered_map<int, ApplicationLaunchData> keyDataMapping;
} Config;

namespace Utils {
  Config parseConfig(Display *display, std::string configFile);
}  // namespace Utils
