#pragma once
#include <functional>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class Ollama {
public:
  json chat(const json &body);
  bool chatStream(const json &body, std::function<bool(const json &)>);
};
