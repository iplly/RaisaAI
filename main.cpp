#include "./src/Skill.h"
#include "./src/VoiceController.h"
#include "./src/debug.h"
#include "control.h"
#include <csignal>
#include <exception>
#include <iostream>
#include <libavcodec/packet.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>

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
    VoiceController voice_controller;
    skill_init();
    debugInit(voice_controller);
    std::jthread(debugConsole).detach();
    voice_controller.Run();
  } catch (std::exception &e) {
    std::cout << "Непонятная ошибка " << e.what() << "\n\n";
    return 1;
  }
  return 0;
}
