#pragma once
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

extern json qwen1_7Data;
void dispatch(const std::string &name, const json &args,
              const std::string &full);
json SkillChoser(const std::string message);