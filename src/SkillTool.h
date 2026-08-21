#pragma once
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
json toolDef(const std::string &name, const std::string &description,
             const json &parameters);
json weatherTool();
json ytmusicTool();
json ytmusicAddTool();
json vkmusicTool();
json vkmusicAddTool();
json vkmusicPlaylistTool();
json timerTool();
