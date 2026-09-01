#include "../Skill.h"
#include "Config.h"
#include <iostream>

std::string LlmSkill::name() const { return "LlmSkill"; }
std::string LlmSkill::description() const { return ""; }
bool LlmSkill::running() const { return busy.load(); }
void LlmSkill::stop() { stopFlag.store(true); }
std::string LlmSkill::execute(json j) {
  std::string message = j["message"];
  std::cout << "message: " << message << "\n\n";
  if (busy)
    return "Ждите";
  if (worker.joinable())
    worker.join();
  busy = true;
  stopFlag = false;
  worker = std::jthread(&LlmSkill::start, this, message);
  return "";
}
void LlmSkill::start(std::string message) {
  try {
    Curl curlLlm(Config::instance().get("OLLAMA_URL") + "/api/chat");
    std::string headers = "Content-Type: application/json";
    json jsonData = {{"model", Config::instance().get("LLM_MODEL")},
                     {"stream", true},
                     {"keep_alive", -1},
                     {"messages", json::array({})}};
    jsonData["messages"].push_back(
        {{"role", "user"}, {"content", message.c_str()}});
    curlLlm.addHeaders(headers);
    curlLlm.post(jsonData, [&](const char *chunk, size_t len) -> size_t {
      std::string sb;
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
        std::string token = j["message"].value("content", "");
        std::cout << token << std::flush;
        stream.full.append(token);
        if (stopFlag)
          return 0;
      }
      return len;
    });
    busy = false;
  } catch (std::exception &e) {
    std::cout << "Ошибка LLM: " << e.what() << "\n\n";
    busy = false;
  }
}
