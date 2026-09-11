#include "core/Config.h"
#include "core/Process.h"
#include "speech/WhisperClient.h"
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

std::string fullWhisper(std::string filePath) {
  std::string out =
      exec("curl -s -F file=@" + filePath + " -F response_format=json " +
           Config::instance().get("WHISPER_URL"));
  return json::parse(out)["text"];
}