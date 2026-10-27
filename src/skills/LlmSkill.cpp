#include "core/Config.h"
#include "core/Json.h"
#include "core/Process.h"
#include "core/Text.h"
#include "net/Curl.h"
#include "playback/TrackQueue.h"
#include "skills/Skill.h"
#include "skills/SkillContext.h"
#include "skills/model/LmTypes.h"
#include "skills/model/llmprompts.h"
#include <cstddef>
#include <exception>
#include <fmt/base.h>
#include <iostream>
#include <optional>
#include <pwd.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unistd.h>

std::string LLMSkill::name() const { return "LlmSkill"; }
std::string LLMSkill::description() const { return ""; }
bool LLMSkill::running() const { return busy.load(); }
void LLMSkill::stop() { stopFlag.store(true); }
std::string LLMSkill::execute(json j) {
  std::string message = j["message"];
  spdlog::info("message: {}", message);

  if (busy)
    return "Ждите";
  if (worker.joinable())
    worker.join();
  busy = true;
  stopFlag = false;
  stream.full.clear();
  worker = std::jthread(&LLMSkill::start, this, message);
  return "";
}

LLMSkill::LLMSkill() {
  LLMTool tool1;
  LLMTool tool2;
  LLMTool tool3;
  LLMTool tool4;
  tool1.function.name = "get_aboba";
  tool1.function.description = getAbobaDescriptionPrompt;
  tool1.function.parameters.properties.insert(
      {"aboba", {"string", getAbobaPropAbobaPrompt}});

  tool2.function.name = "web_search";
  tool2.function.description = webSearchDescriptionPrompt;
  tool2.function.parameters.properties.insert(
      {"query", {"string", webSearchPropQueryPrompt}});

  tool3.function.name = "system_shell";
  tool3.function.description = systemShellDescriptionPrompt;
  tool3.function.parameters.properties.insert(
      {"command", {"string", systemShellPropCommandPrompt}});
  tool4.function.name = "flip_computer";
  tool4.function.description = flipComputerDescriptionPrompt;
  tool4.function.parameters.properties.insert(
      {"angle", {"string", flipComputerPropAnglePrompt}});

  context.messages = {{.role = "system", .content = llmSkillSystemPrompt}};
  context.tools.push_back(std::move(tool1));
  context.tools.push_back(std::move(tool2));
  context.tools.push_back(std::move(tool3));
  context.tools.push_back(std::move(tool4));
  context.options.temperature = 1.5;
  context.options.num_ctx = 65536;
  context.options.top_p = 0.9;
  context.options.top_k = 120;
  context.options.min_p = 0.05;
  context.think = true;
  context.stream = true;
  // json j = context;
  // std::cout << j.dump(2) << "\n\n";
}

void LLMSkill::start(std::string message) {
  try {
    bool done = false;
    Curl curlLlm(Config::instance().get("OLLAMA_URL") + "/api/chat");
    std::string headers = "Content-Type: application/json";

    spdlog::info("Системная информация: {}\n", systemStatusPrompt());
    message.insert(0, systemStatusPrompt() + "\n\n");
    LLMMessage userMessage = {"user", std::move(message)};
    context.messages.push_back(std::move(userMessage));

    curlLlm.addHeaders(headers);

    LLMMessage agentMessage = {"assistant", ""};
    int toolCalls = 0;
    while (!done) {
      bool isToolCall = false;
      json jsonData = context;
      LLMMessage tool;

      std::string sb;
      curlLlm.post(jsonData, [&](const char *chunk, size_t len) -> size_t {
        sb.append(chunk, len);
        size_t pos;

        while ((pos = sb.find('\n')) != std::string::npos) {
          std::string line = sb.substr(0, pos);
          sb.erase(0, pos + 1);

          if (line.empty())
            continue;
          json j;

          try {
            j = json::parse(line);
          } catch (const std::exception &e) {
            continue;
          }

          if (!j.contains("message"))
            continue;

          if (j["message"].contains("tool_calls")) {
            ++toolCalls;
            isToolCall = true;
            stream.full.clear();

            std::cout << j["message"]["tool_calls"] << "\n\n";
            agentMessage.tool_calls = j["message"]["tool_calls"];
            tool = ToolChoser(j["message"]["tool_calls"])[0];

            json toolJson = tool;
            spdlog::info("toolJson start");
            std::cout << toolJson.dump(2) << "\n";
            spdlog::info("toolJson end");
          }
          std::string token = j["message"].value("content", "");
          std::cout << token << std::flush;
          stream.full.append(token);
          agentMessage.content.append(token);
          if (stopFlag)
            return 0;
        }
        return len;
      }); // curlPost end

      if ((!isToolCall || toolCalls > 15)) {
        done = true;
      }
      std::cout << "\n";
      spdlog::info("done: {} toolCalls: {}", done, toolCalls);
      // if (!agentMessage.content.empty()) {
      // size_t pos = agentMessage.content.find(')');
      g_skills.tts->speak(agentMessage.content);
      // }

      context.messages.push_back(std::move(agentMessage));
      context.messages.push_back(tool);
    }

    _lastResponse = stream.full;
    busy = false;
  } catch (std::exception &e) {
    spdlog::error("Ошибка LLM: {}", e.what());
    busy = false;
  }
}

static std::string get_username() {
  uid_t uid = geteuid();
  struct passwd *pw = getpwuid(uid);
  if (pw) {
    return std::string(pw->pw_name);
  }
  return "Unknown";
}

std::string LLMSkill::systemStatusPrompt() {
  using namespace std::chrono;
  Curl curlIP("https://free.freeipapi.com/api/v1/json/");
  Curl curlGeo("https://geocoding-api.open-meteo.com/v1/");
  std::string city = "Санкт-Петербург";
  std::string result;
  curlIP.get("", [&](const char *data, size_t len) -> size_t {
    result.append(data, len);
    return len;
  });
  json curlJson = json::parse(result);
  city = curlJson["cityName"];
  result.clear();
  curlGeo.get("search?name=" + urlEncode(city) + "&count=1&language=ru",
              [&](const char *data, size_t len) -> size_t {
                result.append(data, len);
                return len;
              });
  city = json::parse(result)["results"][0]["admin1"].get<std::string>();
  std::cout << json::parse(result).dump(2) << "\n\n";
  result.clear();

  auto now = system_clock::now();
  zoned_time zt{current_zone(), now};
  auto localNow = zt.get_local_time();
  std::string date = std::format("{:%Y-%m-%d}", localNow);
  std::string time = std::format("{:%H:%M:%S}", localNow);

  Track track = g_skills.vkmusic->nowPlaying();
  std::string trackStr = track.artist + "--" + track.title;

  return fmt::format(
      systemInfoPrompt, fmt::arg("date", date), fmt::arg("time", time),
      fmt::arg("track", track.id.empty() ? "ничего" : trackStr),
      fmt::arg("username", get_username()), fmt::arg("city", city));
}

std::optional<std::string> LLMSkill::lastResponse() { return _lastResponse; }
void LLMSkill::lastResponseReset() { _lastResponse.reset(); }

std::vector<LLMMessage> LLMSkill::ToolChoser(const json &tools) {
  std::vector<LLMMessage> msgTools;
  for (const auto &tool : tools) {
    LLMMessage msg;
    msg.role = "tool";
    msg.tool_call_id = tool["id"].get<std::string>();
    std::string name = tool["function"]["name"].get<std::string>();
    json arguments = tool["function"]["arguments"];
    try {
      if (name == "get_aboba") {
        msg.content =
            "Полученна абоба " + arguments["aboba"].get<std::string>();
      }
      if (name == "web_search") {
        json res = json::parse(exec("python3 src/ddg/ddg.py search " +
                                    arguments["query"].dump()))["results"];
        msg.content = res.dump();
      }
      if (name == "system_shell") {
        msg.content = exec(arguments["command"].get<std::string>());
      }
      if (name == "flip_computer") {
        msg.content = "Компьютер перевёрнут на " +
                      arguments["angle"].get<std::string>() + "градусов";
      }
    } catch (const std::exception &e) {
      spdlog::error("Ошибка вызова инструмента агентом: {}", e.what());
      msg.content = "Ошибка инструмента " + std::string(e.what());
    }
    msgTools.push_back(msg);
  }
  return msgTools;
}
