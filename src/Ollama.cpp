#include "Ollama.h"
#include "curl.h"
#include <iostream>
#include <string>

json Ollama::chat(json body) {
  Curl curlLlm("http://localhost:11434/api/chat");
  std::string headers = "Content-Type: application/json";
  std::string result;

  curlLlm.addHeaders(headers);
  CURLcode res =
      curlLlm.post(body, [&](const char *data, size_t len) -> size_t {
        result.append(data, len);
        return len;
      });
  if (res != CURLE_OK)
    std::cout << "curl: " << curl_easy_strerror(res) << "\n";

  std::cout << "Result: " << result << "\n\n";
  return json::parse(result);
}
//
// bool Ollama::chatStream(json jsonData,
//                         std::function<bool(const json &)> onBody) {
//   try {
//     Curl curlLlm("http://localhost:11434/api/chat");
//     std::string headers = "Content-Type: application/json";
//
//     curlLlm.addHeaders(headers);
//     curlLlm.post(jsonData, [&](const char *chunk, size_t len) -> size_t {
//       std::string sb;
//       sb.append(chunk, len);
//       size_t pos;
//       while ((pos = sb.find('\n')) != std::string::npos) {
//         std::string line = sb.substr(0, pos);
//         sb.erase(0, pos + 1);
//         if (line.empty())
//           continue;
//         json j;
//         try {
//           j = json::parse(line);
//         } catch (const std::exception &e) {
//           continue;
//         }
//       }
//       return len;
//     });
//   } catch (std::exception &e) {
//     std::cout << "Ошибка LLM: " << e.what() << "\n\n";
//   }
// }
