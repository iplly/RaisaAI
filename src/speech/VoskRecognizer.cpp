
#include "speech/VoskRecognizer.h"
#include "nlohmann/json.hpp"
#include <libavcodec/packet.h>
#include <stdexcept>
#include <string>
#include <vosk_api.h>

using json = nlohmann::json;

SpeechRecognizer::SpeechRecognizer(std::string modelPath, double sampleRate) {

  vosk_model = vosk_model_new(modelPath.c_str());
  if (vosk_model == nullptr)
    throw std::runtime_error("Модель не найдена");
  std::string grammar = R"([
  "раиса", "раисы", "раису", "раисе", "раисой", "раечка", "раечки", "раечку", "раечке", "раечкой"])";
  vosk_recognizer = vosk_recognizer_new(vosk_model, sampleRate);
  trigger_vosk_recognizer =
      vosk_recognizer_new_grm(vosk_model, sampleRate, grammar.c_str());
  if (vosk_recognizer == nullptr)
    throw std::runtime_error("Ошибка разпознавателя");
}
int SpeechRecognizer::acceptWaveform(AVPacket packet, bool triggered) {
  return vosk_recognizer_accept_waveform(triggered ? vosk_recognizer
                                                   : trigger_vosk_recognizer,
                                         (char *)packet.data, packet.size);
}
std::string SpeechRecognizer::getPartial() {
  const char *partial_strJson =
      vosk_recognizer_partial_result(trigger_vosk_recognizer);
  auto patrialJson = json::parse(partial_strJson);
  return patrialJson.value("partial", "");
}
std::string SpeechRecognizer::getFull() {
  const char *full_strJson = vosk_recognizer_final_result(vosk_recognizer);
  auto fullJson = json::parse(full_strJson);
  return fullJson.value("text", "");
}
void SpeechRecognizer::reset() {
  vosk_recognizer_reset(vosk_recognizer);
  vosk_recognizer_reset(trigger_vosk_recognizer);
}
SpeechRecognizer::~SpeechRecognizer() {
  vosk_model_free(vosk_model);
  vosk_recognizer_free(vosk_recognizer);
  vosk_recognizer_free(trigger_vosk_recognizer);
}
