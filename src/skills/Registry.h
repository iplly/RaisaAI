#pragma once
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct SkillReg {
  std::string name;
  std::function<json()> tool;
  std::vector<std::string> keywords;
  std::function<void(json)> execute;
};

extern std::vector<SkillReg> g_registry;