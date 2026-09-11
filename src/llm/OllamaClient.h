#pragma once
#include <functional>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class Ollama {
public:
  json chat(json body);
  bool chatStream(json body, std::function<bool(const json &)>);
};
