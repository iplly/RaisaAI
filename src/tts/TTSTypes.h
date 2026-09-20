#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

struct TTSJsonBody {
  std::string input;
  std::string voice = "vivian";
  std::string lang = "Russian";
  std::string response_format = "pcm";
  double temperature = 0.5;
  int top_k = 50;
  double top_p = 0.9;
  double repetition_penalty = 1.2;
  int seed = 42;
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(TTSJsonBody, input, voice, lang,
                                 response_format, temperature, top_k, top_p,
                                 repetition_penalty, seed)
};
