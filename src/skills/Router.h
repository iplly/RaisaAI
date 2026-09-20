#pragma once
#include "skills/model/LmTypes.h"
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

extern LLMRequest router;
json SkillChoser(const std::string &message);
