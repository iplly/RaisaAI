#include "TTSClient.h"
#include "TTSTypes.h"
#include "core/Config.h"
#include "net/Curl.h"
#include "playback/MpvController.h"
#include "playback/VolumeController.h"
#include <csignal>
#include <cstddef>
#include <curl/curl.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using json = nlohmann::json;

TTSClient::TTSClient() {
  if (worker.joinable())
    worker.join();
  worker = std::jthread(&TTSClient::run, this);
}

void TTSClient::speak(const std::string &text) {
  std::lock_guard<std::mutex> lg(mtx);
  spdlog::info("Отправляю в TTS: {}", text);
  queue.push_back(text);
  cv.notify_all();
  spdlog::info("Задача TTS получена");
}

void TTSClient::stop() {
  {
    std::lock_guard<std::mutex> lg(mtx);
    queue.clear();
  }
  pid_t pid = mpvPid.load();
  if (pid > 0)
    kill(-pid, SIGTERM);
  stopFlag = true;
  cv.notify_all();
}

bool TTSClient::generate(const std::string &text) {
  int fds[2];
  pipe(fds);
  pid_t pid = fork();
  if (pid == 0) {
    setpgid(0, 0);
    dup2(fds[0], STDIN_FILENO);
    close(fds[0]);
    close(fds[1]);
    execlp("mpv", "mpv", "--no-terminal", "--no-video",
           "--cache-pause-initial=yes", "--cache-pause-wait=2",
           "--demuxer=rawaudio", "--demuxer-rawaudio-format=s16le",
           "--demuxer-rawaudio-rate=24000", "--demuxer-rawaudio-channels=1",
           "-", nullptr);
    _exit(127);
  }
  close(fds[0]);
  mpvSetVolume(g_volume / 2);
  Curl curl(Config::instance().get("TTS_ULR"));
  curl.addHeaders("Content-Type: application/json");

  TTSJsonBody body{.input = text};
  json bodyJson = body;

  auto res = curl.post(bodyJson, [&](const char *data, size_t len) -> size_t {
    if (stopFlag.load())
      return 0;
    ssize_t written = write(fds[1], data, len);
    return written > 0 ? (size_t)written : 0;
  });
  close(fds[1]);
  waitpid(pid, nullptr, 0);
  mpvPid = -1;
  mpvSetVolume(g_volume);
  return res == CURLE_OK && !stopFlag.load();
}

void TTSClient::run() {
  spdlog::info("TTS Запущен");
  while (true) {
    std::string text;
    {
      std::unique_lock<std::mutex> ul(mtx);
      cv.wait(ul, [&] { return stopFlag || !queue.empty(); });
      if (queue.empty() || stopFlag)
        break;
      text = queue.front();
      queue.pop_front();
    }
    active = true;
    bool ended = generate(text);
    active = false;
    if (ended) {
      stopFlag = false;
      continue;
    }
  }
}
