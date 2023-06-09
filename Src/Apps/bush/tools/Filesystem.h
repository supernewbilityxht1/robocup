#pragma once

#include <string>
#include <vector>

class Filesystem
{
public:
  static std::vector<std::string> getWlanConfigs(const std::string& prefix = "");
  static std::vector<std::string> getLocations(const std::string& prefix = "");
  static std::vector<std::string> getScenarios(const std::string& prefix = "");
  static std::vector<std::string> getProjects(const std::string& prefix = "");
  static std::vector<std::string> getEntries(const std::string& directory,
      bool files,
      bool directories,
      const std::string& suffix = "",
      bool keepSuffixes = true);
};
