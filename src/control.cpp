#include "control.h"
#include <csignal>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <vector>

std::atomic<pid_t> g_vkMpvPid{-1};
std::atomic<uint8_t> g_volume{100};
std::atomic<uint8_t> g_actual{100};
std::atomic<bool> g_pause{false};
std::mutex mtx;

std::string exec(std::string args) {
  char buf[128];
  std::string result;
  struct FileCloser {
    void operator()(FILE *f) const { pclose(f); }
  };
  std::unique_ptr<FILE, FileCloser> pipe(popen(args.c_str(), "r"));
  if (!pipe)
    throw std::runtime_error("Ошибка popen");
  while (fgets(buf, sizeof(buf), pipe.get()) != nullptr) {
    result += buf;
  }
  return result;
}
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

std::string toLowerUtf8(const std::string &s) {
  std::string o = s;
  for (size_t i = 0; i < o.size(); i++) {
    unsigned char c = o[i];
    if (c < 0x80) {
      if (c >= 'A' && c <= 'Z')
        o[i] = c + 32;
      continue;
    }
    unsigned char nxt = (i + 1 < o.size()) ? o[i + 1] : 0;
    if (c == 0xD0 && nxt >= 0x90 && nxt <= 0x9F)
      o[i + 1] = nxt + 0x20; // А-П
    else if (c == 0xD0 && nxt >= 0xA0 && nxt <= 0xAF) {
      o[i] = 0xD1;
      o[i + 1] = nxt - 0x20;
    } // Р-Я
    else if (c == 0xD0 && nxt >= 0x80 && nxt <= 0x8F) {
      o[i] = 0xD1;
      o[i + 1] = nxt + 0x10;
    } // Ѐ-Џ
  }
  return o;
}

static std::vector<std::string> split(const std::string &s) {
  std::vector<std::string> tokens;
  std::istringstream iss(s);
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

int wordsToNumber(std::string w) {
  static const std::map<std::string, int> UNITS = {
      {"один", 1}, {"одна", 1},   {"два", 2},   {"две", 2},
      {"три", 3},  {"четыре", 4}, {"пять", 5},  {"шесть", 6},
      {"семь", 7}, {"восемь", 8}, {"девять", 9}};
  static const std::map<std::string, int> TEENS = {
      {"десять", 10},      {"одиннадцать", 11},  {"двенадцать", 12},
      {"тринадцать", 13},  {"четырнадцать", 14}, {"пятнадцать", 15},
      {"шестнадцать", 16}, {"семнадцать", 17},   {"восемнадцать", 18},
      {"девятнадцать", 19}};
  static const std::map<std::string, int> TENS = {
      {"двадцать", 20},    {"тридцать", 30},   {"сорок", 40},
      {"пятьдесят", 50},   {"шестьдесят", 60}, {"семьдесят", 70},
      {"восемьдесят", 80}, {"девяносто", 90}};
  static const std::map<std::string, int> HUNDREDS = {
      {"сто", 100},       {"двести", 200},    {"триста", 300},
      {"четыреста", 400}, {"пятьсот", 500},   {"шестьсот", 600},
      {"семьсот", 700},   {"восемьсот", 800}, {"девятьсот", 900}};
  static const std::map<std::string, long long> SCALES = {
      {"тысяча", 1000}, {"тысячи", 1000}, {"тысяч", 1000}};

  auto tokens = split(w);
  int total = 0;
  for (const auto &token : tokens) {
    auto itUnit = UNITS.find(token);
    auto itTeen = TEENS.find(token);
    auto itTen = TENS.find(token);
    auto itHundred = HUNDREDS.find(token);
    if (itUnit != UNITS.end()) {
      total += itUnit->second;
    } else if (itTeen != TEENS.end()) {
      total += itTeen->second;
    } else if (itTen != TENS.end()) {
      total += itTen->second;
    } else if (itHundred != HUNDREDS.end()) {
      total += itHundred->second;
    } else {
      auto itScale = SCALES.find(token);
      if (itScale != SCALES.end()) {
        int scale = itScale->second;
        if (total == 0)
          total = 1;
        total += total * scale;
      }
    }
  }
  std::cout << "total: " << total << "\n\n";
  return total;
}

std::string toPrepositional(std::string city) {
  static const std::map<std::string, std::string> ex = {
      {"Москва", "Москве"},
      {"Пермь", "Перми"},
      {"Питер", "Питере"},
      {"Санкт-Петербург", "Санкт-Петербурге"},
      {"Сочи", "Сочи"},
      {"Токио", "Токио"},
      {"Алматы", "Алматы"},
      {"Баку", "Баку"},
      {"Грозный", "Грозном"},
      {"Орёл", "Орле"},
      {"Ростов-на-Дону", "Ростове-на-Дону"},
      {"Нижний Новгород", "Нижнем Новгороде"},
      {"Йошкар-Ола", "Йошкар-Оле"},
  };
  if (auto it = ex.find(city); it != ex.end())
    return it->second;
  return city;
}
