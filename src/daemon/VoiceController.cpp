#include "daemon/VoiceController.h"
#include "core/Config.h"
#include "core/Text.h"
#include "net/Curl.h"
#include "playback/MpvController.h"
#include "playback/VolumeController.h"
#include "skills/Router.h"
#include "skills/SkillContext.h"
#include "speech/WhisperClient.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctre.hpp>
#include <ctre/wrapper.hpp>
#include <exception>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

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

  } else if (ctre::search<"(пауз)">(full) ||
             (ctre::search<"(дальше|включ|продолжи)">(full) && g_pause)) {
    mpvTogglePause();
    return 1;
  } else if (ctre::search<"(тише)">(full)) {
    if (ctre::search<"(?i)в\\s+(\\S+)\\s+раз">(full)) {
      int volume = wordsToNumber(full);
      if (volume != 0)
        setVolume(std::max(0, g_volume / volume));
      else
        setVolume(0);
    } else
      setVolume(std::max(0, g_volume - 15));
    mpvSetVolume(g_volume);
    spdlog::info("g_volume: {}", (int)g_volume);
    return 1;

  } else if (ctre::search<"(громче)">(full)) {
    if (ctre::search<"(?i)в\\s+(\\S+)\\s+раз">(full)) {
      int volume = wordsToNumber(full);
      setVolume(std::min(100, g_volume * volume));
    } else
      setVolume(std::min(100, g_volume + 15));
    mpvSetVolume(g_volume);
    spdlog::info("g_volume: {}", (int)g_volume);
    return 1;

  } else if (ctre::search<"(громкость|звук на)">(full)) {
    int volume = wordsToNumber(full);
    if (volume <= 10)
      setVolume(volume * 10);
    else if (volume > 100)
      setVolume(100);
    else
      setVolume(volume);
    mpvSetVolume(g_volume);
    spdlog::info("g_volume: {}", (int)g_volume);
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
    : audio(Config::instance().get("AUDIO_RATE"),
            Config::instance().get("AUDIO_CHANNELS"),
            Config::instance().get("AUDIO_DEVICE")),
      vosk(Config::instance().get("VOSK_PATH"),
           std::stof(Config::instance().get("AUDIO_RATE"))),
      tts() {
  curl_global_init(CURL_GLOBAL_DEFAULT);
}

void VoiceController::Run() {
  std::thread(&VoiceController::processor, this).detach();
  listener();
  running = false;
  taskQueue.stop();
}

void VoiceController::listener() {
  LLMSkill *llm = g_skills.llmskill.get();
  bool triggered = 0;
  AVPacket packet = {};
  std::vector<uint8_t> audioBuffer;
  audioBuffer.reserve(2000000);
  int ringIndex = -1;

  while (running) {
    try {
      audio.read(packet);
      if (triggered == 1)
        audioBuffer.insert(audioBuffer.end(), packet.data,
                           packet.data + packet.size);

      int status = vosk.acceptWaveform(packet, triggered);

      std::string speech = vosk.getPartial();

      if (llm->lastResponse().has_value()) {
        tts.speak(llm->lastResponse().value());
        llm->lastResponseReset();
      }

      if (ctre::search<"(раиса|раечка)">(speech) && triggered == 0) {
        mpvSetVolume(g_volume / 2);
        spdlog::info("g_volume: {}", (int)g_volume);
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
      spdlog::error("Неизвестная ошибка: {}", e.what());
      continue;
    }
  }
}

void VoiceController::processor() {
  using namespace std::chrono;
  LLMSkill *llm = g_skills.llmskill.get();
  while (running) {
    try {
      processing = false;
      std::string filePath = taskQueue.pop();
      if (filePath.empty())
        break;
      auto start = steady_clock::now();
      processing = true;
      std::string full;
      try {
        full = fullWhisper(filePath);
      } catch (const std::exception &e) {
        spdlog::error("Ошибка Whisper: {}", e.what());
      }
      spdlog::info("user message: {}", full);

      if (!handleCommand(full)) {

        spdlog::info("Обрабатывю запрос");
        json tool = SkillChoser(full);
        if (skip.exchange(false)) {
          spdlog::warn("Задача прервана");
          continue;
        }
        if (tool.is_null()) {
          if (!llm->running()) {
            json message = {{"message", full}};
            llm->execute(message);
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
          tts.speak(g_skills.vkmusic->execute(arguments));
        }
        if (name == "TimerSkill") {
          g_skills.timerskill->execute(arguments);
        }

        skip = false;
      }
      auto end = steady_clock::now();
      auto diff = duration_cast<seconds>(end - start).count();
      spdlog::info("Время выполнения: {}", diff);
    } catch (const std::exception &e) {
      spdlog::error("Ошибка обработчика: {}", e.what());
    }
  }
}
