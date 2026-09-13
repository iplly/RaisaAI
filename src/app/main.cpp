#include "core/Config.h"
#include "core/Paths.h"
#include "daemon/Console.h"
#include "daemon/VoiceController.h"
#include "playback/MpvController.h"
#include "playback/VolumeController.h"
#include "skills/SkillContext.h"
#include <algorithm>
#include <csignal>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <libavcodec/packet.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

static void shutdownHandler(int) {
  pid_t p = g_vkMpvPid.load();
  if (p > 0)
    kill(p, SIGTERM); // mpv гаснет
  _exit(0);
}

int main() {
  try {
    signal(SIGTERM, shutdownHandler);
    signal(SIGINT, shutdownHandler);
    Config::instance().load();
    std::ifstream in(dirControl + "volume");
    int volume = 100;
    if (in >> volume)
      volume = std::clamp(volume, 0, 100);
    g_volume = static_cast<uint8_t>(volume);
    spdlog::info("g_volume: {}", g_volume.load());
    VoiceController voice_controller;
    skill_init();
    debugInit(voice_controller);
    std::jthread(debugConsole).detach();
    voice_controller.Run();
  } catch (std::exception &e) {
    spdlog::critical("Непонятная ошибка: ", e.what());
    return 1;
  }
  return 0;
}
