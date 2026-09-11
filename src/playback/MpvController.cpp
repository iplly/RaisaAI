#include "playback/MpvController.h"
#include "core/Process.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

std::atomic<pid_t> g_vkMpvPid{-1};
std::atomic<bool> g_pause{false};

static void smoothSetVolume(char target, pid_t pid) {
  std::lock_guard<std::mutex> lock(mtx);
  uint8_t current = g_actual;
  if (current == target)
    return;
  while (current != target) {
    if (current > target)
      --current;
    else
      ++current;
    mpvSetVolume_(current, pid);
    g_actual = current;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}
void mpvSetVolume(char v, pid_t pid) {
  std::thread([v, pid]() { smoothSetVolume(v, pid); }).detach();
}

void mpvSetVolume_(char v, pid_t pid) {
  if (g_vkMpvPid <= 0)
    return;
  if (kill(g_vkMpvPid, 0) != 0) {
    g_vkMpvPid = -1;
    return;
  }
  std::string cmd = "printf 'set volume " + std::to_string(v) +
                    "\\n' | socat -t2 - ABSTRACT-CONNECT:raisa-mpv-" +
                    std::to_string(pid) + ".sock";
  exec(cmd);
}

pid_t spawnMPV(const std::string &url, uint8_t volume) {
  pid_t pid = fork();
  if (pid == 0) {
    setsid();
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    std::string volArgs = "--volume=" + std::to_string(volume);
    execlp(
        "mpv", "mpv", "--no-video", "--network-timeout=5",
        "--demuxer-lavf-o=http_persistent=0",
        ("--input-ipc-server=@raisa-mpv-" + std::to_string(getpid()) + ".sock")
            .c_str(),
        url.c_str(), volArgs.c_str(), nullptr);
    exit(127);
  }
  return pid;
}
void mpvTogglePause(pid_t pid) {
  if (g_vkMpvPid <= 0)
    return;
  if (kill(g_vkMpvPid, 0) != 0) {
    g_vkMpvPid = -1;
    return;
  }
  bool old = g_pause;
  while (!g_pause.compare_exchange_strong(old, !old))
    ;

  exec("printf 'cycle pause\\n' | "
       "socat -t2 - ABSTRACT-CONNECT:raisa-mpv-" +
       std::to_string(pid) + ".sock");
}
