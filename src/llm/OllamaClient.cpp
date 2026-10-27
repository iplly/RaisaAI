#include "llm/OllamaClient.h"
#include "core/Config.h"
#include "net/Curl.h"
#include <exception>
#include <spdlog/spdlog.h>
#include <string>

json Ollama::chat(const json &body) {
  try {
    Curl curlLlm(Config::instance().get("OLLAMA_URL") + "/api/chat");
    std::string headers = "Content-Type: application/json";
    std::string result;

    curlLlm.addHeaders(headers);
    curlLlm.post(body, [&](const char *data, size_t len) -> size_t {
      result.append(data, len);
      return len;
    });

    return json::parse(result);
  } catch (const std::exception &e) {
    spdlog::error("Ошибка доступа к ollama: {}", e.what());
    return {};
  }
}
