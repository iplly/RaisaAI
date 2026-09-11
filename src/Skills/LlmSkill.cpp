#include "../Skill.h"
#include "Config.h"
#include "control.h"
#include <iostream>
#include <string>

std::string LLMSkill::name() const { return "LlmSkill"; }
std::string LLMSkill::description() const { return ""; }
bool LLMSkill::running() const { return busy.load(); }
void LLMSkill::stop() { stopFlag.store(true); }
std::string LLMSkill::execute(json j) {
  std::string message = j["message"];
  std::cout << "message: " << message << "\n\n";
  if (busy)
    return "Ждите";
  if (worker.joinable())
    worker.join();
  busy = true;
  stopFlag = false;
  worker = std::jthread(&LLMSkill::start, this, message);
  return "";
}

LLMSkill::LLMSkill() {
  LLMTool tool;
  tool.function.name = "get_aboba";
  tool.function.description =
      "Пользователь просит получить абобу, абоба "
      "предоставляется в виде строки, после получения ответь пользователю "
      "что абоба получена, абоба получается исключительно один раз за сессию";
  tool.function.parameters.properties = {
      {"aboba",
       {"string", "абоба, необходимо указывать если не сказано иное"}}};
  context.tools.push_back(std::move(tool));
}

void LLMSkill::start(std::string message) {
  try {
    bool done = false;
    Curl curlLlm(Config::instance().get("OLLAMA_URL") + "/api/chat");
    std::string headers = "Content-Type: application/json";
    LLMMessage userMessage = {"user", message};
    context.messages.push_back(std::move(userMessage));

    curlLlm.addHeaders(headers);

    LLMMessage agentMessage = {"assistant", ""};
    int toolCalls = 0;
    while (!done) {
      bool isToolCall = false;
      json jsonData = context;
      std::cout << jsonData.dump(2) << "\n\n";
      LLMMessage tool;

      std::string sb;
      curlLlm.post(jsonData, [&](const char *chunk, size_t len) -> size_t {
        stream.full.clear();
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

            std::cout << j["message"]["tool_calls"] << "\n\n";
            agentMessage.tool_calls = j["message"]["tool_calls"];
            tool = ToolChoser(j["message"]["tool_calls"])[0];

            json toolJson = tool;
            std::cout << toolJson.dump(2) << "\n\n";
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

      if ((!isToolCall || toolCalls >= 2)) {
        done = true;
      }
      std::cout << "\ndone: " << done << " toolCalls: " << toolCalls << "\n\n";

      context.messages.push_back(std::move(agentMessage));
      context.messages.push_back(tool);
    }

    // std::cout << stream.full << "\n\n";

    busy = false;
  } catch (std::exception &e) {
    std::cout << "Ошибка LLM: " << e.what() << "\n\n";
    busy = false;
  }
}
std::vector<LLMMessage> LLMSkill::ToolChoser(json tools) {
  std::vector<LLMMessage> msgTools;
  for (const auto &tool : tools) {
    LLMMessage msg;
    if (tool["function"]["name"] == "get_aboba") {
      msg.role = "tool";
      msg.content = "Полученна абоба " +
                    tool["function"]["arguments"]["aboba"].get<std::string>();
      msg.tool_call_id = tool["id"].get<std::string>();
    }
    msgTools.push_back(msg);
  }
  return msgTools;
}
