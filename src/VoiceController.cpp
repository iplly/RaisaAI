#include "VoiceController.h"
#include "Skill.h"
#include "control.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctre.hpp>
#include <ctre/wrapper.hpp>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static std::string fullWhisper(std::string filePath) {
  std::string out = exec("curl -s -F file=@" + filePath +
                         " -F response_format=json "
                         "http://127.0.0.1:8000/inference");
  return json::parse(out)["text"];
}

bool VoiceController::quickCommand(const std::string &full) {
  if (ctre::search<"(стоп|остановись)">(full)) {
    if (processing) {
      skip = true;
      return 1;
    }
    if (g_skills.llmskill->running())
      g_skills.llmskill->stop();
    else if (g_skills.vkmusic->running())
      g_skills.vkmusic->stop();
    return 1;

  } else if (ctre::search<"(пауз)">(full)) {
    mpvTogglePause();
    return 1;
  } else if (ctre::search<"(дальше|включ|продолжи)">(full) && g_pause) {
    mpvTogglePause();
    return 1;
  } else if (ctre::search<"(тише)">(full)) {
    if (ctre::search<"(?i)в\\s+(\\S+)\\s+раза">(full)) {
      int volume = wordsToNumber(full);
      g_volume = std::max(0, g_volume / volume);
    } else
      g_volume = std::max(0, g_volume - 15);
    mpvSetVolume(g_volume);
    std::cout << "g_volume: " << (int)g_volume << "\n\n";
    return 1;

  } else if (ctre::search<"(громче)">(full)) {
    if (ctre::search<"(?i)в\\s+(\\S+)\\s+раза">(full)) {
      int volume = wordsToNumber(full);
      g_volume = std::min(100, g_volume * volume);
    } else
      g_volume = std::min(100, g_volume + 15);
    mpvSetVolume(g_volume);
    std::cout << "g_volume: " << (int)g_volume << "\n\n";
    return 1;

  } else if (ctre::search<"(громкость|звук на)">(full)) {
    int volume = wordsToNumber(full);
    if (volume <= 10)
      g_volume = volume * 10;
    else if (volume > 100)
      g_volume = 100;
    else
      g_volume = volume;
    mpvSetVolume(g_volume);
    std::cout << "g_volume: " << (int)g_volume << "\n\n";
    return 1;

  } else if (ctre::search<"(следущ|пропусти|дальше)">(full)) {
    g_skills.vkmusic->next();
    return 1;

  } else if (ctre::search<"(перемеш)">(full)) {
    g_skills.vkmusic->shuffleQueue();
    return 1;
  } else if (ctre::search<"(очисти очедедь)">(full)) {
    g_skills.vkmusic->clearSecond();
  }
  return 0;
}

bool VoiceController::handleCommand(const std::string &full) {
  std::string fullLower = toLowerUtf8(full);
  if (ctre::search<"(добавь|в очередь)">(fullLower) &&
      !ctre::search<"(таймер)">(fullLower)) {
    if (!g_skills.vkmusic->running())
      return 1;

    if (ctre::search<"(в конец)">(fullLower)) {
      std::thread([full]() { g_skills.vkmusic->addToEnd(full); }).detach();
      return 1;
    }
    std::thread([full]() { g_skills.vkmusic->add(full); }).detach();
    return 1;
  }
  // TODO: Сводка погоды тут
  return 0;
}

VoiceController::VoiceController()
    : audio("48000", "1", "Raisa"),
      vosk("./Models/vosk-model-small/", 48000.0) {}

void VoiceController::Run() {
  std::thread(&VoiceController::processor, this).detach();
  listener();
  running = false;
  taskQueue.stop();
}

void VoiceController::listener() {
  bool triggered = 0;
  AVPacket packet = {};
  std::vector<uint8_t> audioBuffer;
  audioBuffer.reserve(2000000);
  LlmSkill *llm = g_skills.llmskill.get();
  int ringIndex = -1;

  while (running) {
    try {
      audio.read(packet);
      if (triggered == 1)
        audioBuffer.insert(audioBuffer.end(), packet.data,
                           packet.data + packet.size);
      int status = vosk.acceptWaveform(packet);

      std::string speech = vosk.getPartial();

      if (speech != "" && !llm->running())
        std::cout << speech << " " << status << "\n";

      if (ctre::search<"(раиса|рая|раечка)">(speech) && triggered == 0) {
        mpvSetVolume(g_volume / 2);
        std::cout << "g_volume: " << (int)g_volume << "\n\n Бухва: ";
        triggered = 1;
        vosk.reset();
        continue;
      }

      else if (status == 1 && triggered == 1) {
        std::string full = vosk.getFull();
        if (!quickCommand(full)) {
          (++ringIndex) %= 10;
          audio.saveWav("/tmp/raisa_" + std::to_string(ringIndex) + ".wav",
                        audioBuffer);
          taskQueue.push("/tmp/raisa_" + std::to_string(ringIndex) + ".wav");
        }
        goto nahui;
      } else if (status == 1 && triggered == 0) {
      nahui:
        mpvSetVolume(g_volume);
        triggered = 0;
        speech.clear();
        vosk.reset();
        audioBuffer.clear();
        av_packet_unref(&packet);
      }
    } catch (std::exception &e) {
      std::cout << "Неизветная ошибка " << e.what() << "\n\n";
      continue;
    }
  }
}

void VoiceController::TaskQueue::push(std::string s) {
  std::lock_guard<std::mutex> lg(mtx);
  tasks.push_back(s);
  cv.notify_one();
}
std::string VoiceController::TaskQueue::pop() {
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait(lock, [&] { return !tasks.empty() || stopped; });
  std::string task;
  if (!tasks.empty()) {
    task = tasks.front();
    tasks.pop_front();
  }
  return task;
}
void VoiceController::TaskQueue::stop() {
  stopped = true;
  cv.notify_all();
}

void VoiceController::processor() {
  using namespace std::chrono;
  LlmSkill *llm = g_skills.llmskill.get();
  while (running) {
    processing = false;
    std::string filePath = taskQueue.pop();
    if (filePath.empty())
      break;
    auto start = steady_clock::now();
    processing = true;
    std::string full = fullWhisper(filePath);
    std::cout << "user message: " << full << "\n\n";

    if (!handleCommand(full)) {

      std::cout << "Обрабатывю запрос" << "\n ";
      json tool = SkillChoser(full);
      if (skip.exchange(false)) {
        std::cout << "Задача прервана" << "\n\n";
        continue;
      }
      std::cout << "tool: " << tool << "\n\n";
      if (tool.is_null()) {
        if (!llm->running()) {
          json message = {{"message", full}};
          std::cout << llm->execute(message);
        }
        continue;
      }
      std::string name = tool["function"]["name"];
      json arguments = tool["function"]["arguments"];
      if (name == "WeatherSkill") {
        arguments["detals"] = true;
        g_skills.weather->execute(arguments);
      }
      if (name == "VKMusicSkill") {
        g_skills.vkmusic->execute(arguments);
      }
      if (name == "TimerSkill") {
        g_skills.tiemrskill->execute(arguments);
      }

      skip = false;
    }
    auto end = steady_clock::now();
    auto diff = duration_cast<seconds>(end - start).count();
    std::cout << "Время выполнения: " << diff << "s\n\n";
  }
}
