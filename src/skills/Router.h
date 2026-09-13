#pragma once
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

extern json qwen1_7Data;
json SkillChoser(const std::string &message);
