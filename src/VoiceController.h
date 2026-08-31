#pragma once
#include "audio.h"
#include "vosk.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

class VoiceController {
  AudioController audio;
  SpeechRecognizer vosk;
  std::atomic<bool> skip{false};
  std::atomic<bool> processing{false};
  struct TaskQueue {
    std::mutex mtx;
    std::condition_variable cv;
    std::deque<std::string> tasks;
    std::atomic<bool> stopped{false};
    void push(std::string);
    std::string pop();
    void stop();
  };
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
