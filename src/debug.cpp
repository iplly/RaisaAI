#include "debug.h"
#include "Skill.h"
#include "VoiceController.h"
#include "control.h"
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

struct dbCMD {
  std::function<void(std::vector<std::string>)> fn;
};
static std::map<std::string, dbCMD> g_dbg;

static std::string join(const std::vector<std::string> &v) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i)
      out += " ";
    out += v[i];
  }
  return out;
}

void debugInit(VoiceController &vc) {
  g_dbg["help"] = {[](auto) { /* печать всех сигнатур */ }};
  g_dbg["status"] = {[](auto) {
    std::cout << "g_volume: " << (int)g_volume << "\n";
    std::cout << "g_pause: " << g_pause << "\n";
    std::cout << "TrackQueue: \nОчередь Primary: \n";
    for (auto [title, artist, id] : g_skills.vkmusic->trackQueue.primary)
      std::cout << "  " << title << " -- " << artist << " : " << id << "\n";
    std::cout << "Очередь Second: \n";
    for (auto [title, artist, id] : g_skills.vkmusic->trackQueue.second)
      std::cout << "  " << title << " -- " << artist << " : " << id << "\n";
    std::cout << "Таймеры/будильники:\n";
    for (auto &[when, name, task, period, periodDays] :
         g_skills.timerskill->timers) {
      auto hours =
          std::chrono::duration_cast<std::chrono::hours>(when - Clock::now());
      auto mins = std::chrono::duration_cast<std::chrono::minutes>(
          when - Clock::now() - hours);
      auto sec = std::chrono::duration_cast<std::chrono::seconds>(
          when - Clock::now() - mins);

      std::cout << "  " << name << " -- " << mins.count() << ":" << sec.count()
                << "\n";
    }
  }};
  g_dbg["vol"] = {[](auto a) {
    g_volume = stoi(a[0]);
    mpvSetVolume(g_volume);
  }};
  g_dbg["pause"] = {[](auto) { mpvTogglePause(); }};
  g_dbg["stop"] = {[&vc](auto) {
    if (vc.processing) {
      vc.skip = true;
    }
    if (g_skills.llmskill->running())
      g_skills.llmskill->stop();
    else if (g_skills.vkmusic->running())
      g_skills.vkmusic->stop();
  }};
  g_dbg["next"] = {[](auto) { g_skills.vkmusic->next(); }};
  g_dbg["shuffle"] = {[](auto) { g_skills.vkmusic->shuffleQueue(); }};
  g_dbg["clear"] = {[](auto) { g_skills.vkmusic->clearPrimary(); }};
  g_dbg["vk.add"] = {[](auto a) { g_skills.vkmusic->add(join(a)); }};
  g_dbg["weather"] = {
      [](auto a) { g_skills.weather->execute({{"city", join(a)}}); }};
  g_dbg["timer"] = {[](auto a) {
    g_skills.timerskill->execute({{"type", "timer"}, {"minutes", stoi(a[0])}});
  }};
  g_dbg["alarm"] = {[](auto a) {
    g_skills.timerskill->execute({{"type", "alarm"}, {"time", a[0]}});
  }};
  g_dbg["llm"] = {[](auto a) {
    std::cout << g_skills.llmskill->execute({{"message", join(a)}});
  }};
}

void debugConsole() {
  std::string line;
  while (std::getline(std::cin, line)) { // EOF (systemd) → выход
    auto pos = line.find_first_of(" \t");
    std::string sig = line.substr(0, pos);
    std::vector<std::string> args; // остаток строки → аргументы
    std::string rest = (pos == std::string::npos) ? "" : line.substr(pos + 1);
    size_t p;
    while ((p = rest.find_first_of(" \t")) != std::string::npos) {
      if (!rest.substr(0, p).empty())
        args.push_back(rest.substr(0, p));
      rest.erase(0, p + 1);
    }
    if (!rest.empty())
      args.push_back(rest);
    if (auto it = g_dbg.find(sig); it != g_dbg.end())
      it->second.fn(args);
    else
      std::cout << "Неизвестная команда. help\n\n";
  }
}
