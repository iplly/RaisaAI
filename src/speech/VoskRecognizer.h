#pragma once
#include "nlohmann/json.hpp"
#include <libavcodec/packet.h>
#include <stdexcept>
#include <string>
#include <vosk_api.h>

class SpeechRecognizer {
  VoskRecognizer *vosk_recognizer;
  VoskRecognizer *trigger_vosk_recognizer;
  VoskModel *vosk_model;

public:
  SpeechRecognizer(std::string modelPath, double sampleRate);
  int acceptWaveform(AVPacket packet, bool triggered = false);
  int triggerAcceptWaveform(AVPacket packet);
  std::string getPartial();
  std::string getFull();
  void reset();
  SpeechRecognizer(SpeechRecognizer const &) = delete;
  SpeechRecognizer &operator=(SpeechRecognizer const &) = delete;
  SpeechRecognizer(SpeechRecognizer &&) = delete;
  SpeechRecognizer &operator=(SpeechRecognizer &&) = delete;
  ~SpeechRecognizer();
};
