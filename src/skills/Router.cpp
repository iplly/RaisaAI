#include "skills/Router.h"
#include "llm/OllamaClient.h"
#include "skills/SkillContext.h"
#include "spdlog/spdlog.h"
#include <exception>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

json qwen1_7Data =
    "    {\"model\":\"\",\"keep_alive\":-1,\"messages\":[{\"role\": "
    "\"system\", \"content\":\"Ты — роутер команд голосового ассистента Raisa. "
    "Просьба про музыку (включи, поставь, вруби, послушай, песня, трек, "
    "исполнитель, группа, альбом, микс, жанр) → инструмент VKMusicSkill. В "
    "track пиши ТОЛЬКО название, выбросив служебные слова в начале       "
    "фразы: включи, поставь, вруби, песню, трек, музыку и т.п. Пример: «включи "
    "песню Кино группа крови» → VKMusicSkill, track=\\\"Кино группа"
    "крови\\\". Про погоду или температуру → WeatherSkill.\"}],\"think\":false,\"tools\":[],\"tool_choice\":\"required\",\"stream\":false}"_json;

json SkillChoser(const std::string &message) {
  Ollama ollama;
  try {
    json jsonData = qwen1_7Data;
    jsonData["tools"].push_back(g_skills.weather->tool());
    jsonData["tools"].push_back(g_skills.vkmusic->tool());
    jsonData["tools"].push_back(g_skills.timerskill->tool());

    jsonData["messages"].push_back(
        {{"role", "user"}, {"content", message.c_str()}});

    json j = ollama.chat(jsonData)["message"]["tool_calls"];

    spdlog::info("Вызванный инструмент: {}, Параметры: {}",
                 j[0]["function"]["name"].dump(),
                 j[0]["function"]["arguments"].dump());

    return (j.empty()) ? json(nullptr) : j[0];
  } catch (const std::exception &e) {
    spdlog::error("Ошибка выбора инструмента: {}", e.what());
    return {};
  }
}
