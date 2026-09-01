#include "Skill.h"
#include "Config.h"
#include "Ollama.h"
#include "SkillTool.h"
#include "control.h"
#include "curl.h"
#include <ctre.hpp>
#include <ctre/wrapper.hpp>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using json = nlohmann::json;

struct Skills g_skills;
std::vector<SkillReg> g_registry = {
    {"WeatherSkill",
     weatherTool,
     {"сводк", "подробн", "весь день"},
     [](json a) { g_skills.weather->execute(a); }},
    {"VKMusicSkill",
     vkmusicTool,
     {},
     [](json a) { g_skills.vkmusic->execute(a); }},
    {"TimerSkill", timerTool, {}, [](json a) {
       g_skills.timerskill->execute(a);
     }}};

json qwen1_7Data =
    "    {\"model\":\"\",\"keep_alive\":-1,\"messages\":[{\"role\": "
    "\"system\", \"content\":\"Ты — роутер команд голосового ассистента Raisa. "
    "Просьба про музыку (включи, поставь, вруби, послушай, песня, трек, "
    "исполнитель, группа, альбом, микс, жанр) → инструмент VKMusicSkill. В "
    "track пиши ТОЛЬКО название, выбросив служебные слова в начале       "
    "фразы: включи, поставь, вруби, песню, трек, музыку и т.п. Пример: «включи "
    "песню Кино группа крови» → VKMusicSkill, track=\\\"Кино группа"
    "крови\\\". Про погоду или температуру → WeatherSkill.\"}],\"think\":false,\"tools\":[],\"tool_choice\":\"required\",\"stream\":false}"_json;

void skill_init() {
  qwen1_7Data["model"] = Config::instance().get("ROUTER_MODEL");
  g_skills.weather = std::make_unique<WeatherSkill>();
  // g_skills.ytmusic = std::make_unique<YTMusicSkill>();
  g_skills.llmskill = std::make_unique<LlmSkill>();
  g_skills.vkmusic = std::make_unique<VKMusicSkill>();
  g_skills.timerskill = std::make_unique<TimerSkill>();
}

json SkillChoser(std::string message) {
  std::string answer;
  std::string result;
  Ollama ollama;
  try {
    json jsonData = qwen1_7Data;
    for (const auto &r : g_registry)
      jsonData["tools"].push_back(r.tool());
    jsonData["messages"].push_back(
        {{"role", "user"}, {"content", message.c_str()}});

    json j = ollama.chat(jsonData)["message"]["tool_calls"];

    return (j.empty()) ? json(nullptr) : j[0];
  } catch (const std::exception &e) {
    std::cout << "Ошибка выбора инструмента " << e.what() << "\n\n";
    return {};
  }
}

void dispatch(const std::string &name, json &args, const std::string &full) {
  for (const auto &r : g_registry) {
    if (r.name != name)
      continue;
    std::string low = toLowerUtf8(full);
    for (auto &w : r.keywords) {
      if (low.find(w) != std::string::npos)
        args["detals"] = "full";
      r.execute(args);
      return;
    }
  }
}

std::string urlEncode(const std::string &s) {
  static const char *hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size() * 3);
  for (unsigned char c : s) { // unsigned — иначе сравнения сломаются
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~')
      out += c; // алифанумерика и -._~ — как есть
    else {
      out += '%';
      out += hex[c >> 4]; // старшая половина байта → %XX
      out += hex[c & 0xF];
    }
  }
  return out;
}
