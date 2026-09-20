#pragma once
#include "audio/AudioCapture.h"
#include "daemon/TaskQueue.h"
#include "speech/VoskRecognizer.h"
#include "tts/TTSClient.h"
#include <atomic>
#include <string>

class VoiceController {
  AudioController audio;
  SpeechRecognizer vosk;
  TTSClient tts;
  std::atomic<bool> skip{false};
  std::atomic<bool> processing{false};
  TaskQueue taskQueue;
  std::atomic<bool> running{true};

  void listener();
  void processor();
  bool quickCommand(const std::string &full);
  bool handleCommand(const std::string &full);

public:
  friend void debugInit(VoiceController &vc);
  VoiceController();
  void Run();
  VoiceController(VoiceController const &) = delete;
  VoiceController &operator=(VoiceController const &) = delete;
  VoiceController(VoiceController &&) = delete;
  VoiceController &operator=(VoiceController &&) = delete;
};
