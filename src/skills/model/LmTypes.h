#pragma once
#include "core/Json.h"
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

class LLMTool {
  class Function {
    class Parameters {
      struct prop {
        std::string type;
        std::string description;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(prop, type, description)
      };

      const std::string type = "object";

    public:
      std::vector<std::string> required;
      std::unordered_map<std::string, prop> properties;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE(Parameters, type, required, properties)
    };

  public:
    std::string name;
    std::string description;
    Parameters parameters;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Function, name, description, parameters)
  };

public:
  Function function;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(LLMTool, type, function)
private:
  const std::string type = "function";
};

struct LLMMessage {
  std::string role;
  std::string content;
  std::optional<std::string> tool_call_id = std::nullopt;
  std::optional<std::vector<json>> tool_calls = std::nullopt;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(LLMMessage, role, content, tool_call_id,
                                 tool_calls)
};

class LLMRequest {
  struct Options {
    double temperature;
    int top_k;
    double top_p;
    double min_p;
    int num_ctx;
    double num_predict;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Options, temperature, top_k, top_p, min_p,
                                   num_ctx, num_predict)
  };

public:
  std::string model;
  bool stream = false;
  int keep_alive = -1;
  bool think = false;
  Options options;
  std::vector<LLMMessage> messages;
  std::vector<LLMTool> tools;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(LLMRequest, model, stream, keep_alive,
                                 messages, tools, options, think)
};
