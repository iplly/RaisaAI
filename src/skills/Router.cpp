#include "skills/Router.h"
#include "llm/OllamaClient.h"
#include "skills/SkillContext.h"
#include "spdlog/spdlog.h"
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

LLMRequest router;

json SkillChoser(const std::string &message) {
  Ollama ollama;
  try {
    json jsonData = router;
    jsonData["tools"].push_back(g_skills.weather->tool());
    jsonData["tools"].push_back(g_skills.vkmusic->tool());
    jsonData["tools"].push_back(g_skills.timerskill->tool());

    jsonData["messages"].push_back(
        {{"role", "user"}, {"content", message.c_str()}});

    json j = ollama.chat(jsonData);
    json tool = j["message"]["tool_calls"];

    spdlog::info("{}", j["message"]["content"].get<std::string>());
    if (!tool.is_null()) {
      spdlog::info("Вызванный инструмент: {}, Параметры: {}",
                   tool[0]["function"]["name"].dump(),
                   tool[0]["function"]["arguments"].dump());
    }

    return (tool.empty()) ? json(nullptr) : tool[0];
  } catch (const std::exception &e) {
    spdlog::error("Ошибка выбора инструмента: {}", e.what());
    return {};
  }
}
