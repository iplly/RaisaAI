#include "core/Config.h"
#include "core/Json.h"
#include "core/Process.h"
#include "net/Curl.h"
#include "playback/TrackQueue.h"
#include "skills/Skill.h"
#include "skills/SkillContext.h"
#include "skills/model/LmTypes.h"
#include "skills/model/llmprompts.h"
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

  context.messages = {{.role = "system", .content = llmSkillSystemPrompt}};
  context.tools.push_back(std::move(tool1));
  context.tools.push_back(std::move(tool2));
  context.tools.push_back(std::move(tool3));
  context.options.temperature = 0.7;
  context.options.num_ctx = 65536;
  context.options.top_p = 0.9;
  context.think = true;
  context.stream = true;
}

void LLMSkill::start(std::string message) {
  try {
    bool done = false;
    Curl curlLlm(Config::instance().get("OLLAMA_URL") + "/api/chat");
    std::string headers = "Content-Type: application/json";
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

      context.messages.push_back(std::move(agentMessage));
      context.messages.push_back(tool);
    }

    // std::cout << stream.full << "\n\n";

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
  auto now = std::chrono::system_clock::now();
  std::string date = std::format("{:%Y-%m-%d}", now);
  std::string time = std::format("{:%H:%M:%S}", now);
  Track track = g_skills.vkmusic->nowPlaying();
  std::string trackStr = track.artist + "--" + track.title;
  return fmt::format(systemInfoPrompt, fmt::arg("date", date),
                     fmt::arg("time", time),
                     fmt::arg("track", track.id.empty() ? "ничего" : trackStr),
                     fmt::arg("username", get_username()));
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
    } catch (const std::exception &e) {
      spdlog::error("Ошибка вызова инструмента агентом: {}", e.what());
      msg.content = "Ошибка инструмента " + std::string(e.what());
    }
    msgTools.push_back(msg);
  }
  return msgTools;
}
