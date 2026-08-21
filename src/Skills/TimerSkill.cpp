#include "../Skill.h"
#include "control.h"
#include <bits/chrono.h>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <utility>

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

static TimePoint strToTime(const std::string &time) {
  using namespace std::chrono;
  auto ddot = time.find(':');
  if (ddot == std::string::npos)
    throw std::runtime_error("Ошибка преобразования времени");
  auto h = hours(std::stoi(time.substr(0, ddot)));
  auto m = minutes(std::stoi(time.substr(ddot + 1, std::string::npos)));
  auto now = Clock::now();

  zoned_time zt{current_zone(), now};
  auto localNow = zt.get_local_time();

  auto targetLocal = floor<days>(localNow) + h + m;
  zoned_time targetZt{current_zone(), targetLocal};

  return targetZt.get_sys_time();
}

static Clock::time_point nextAlarm(TimePoint time) {
  auto now = Clock::now();
  auto today = time;
  return today > now ? today : today + std::chrono::days{1};
}

std::string TimerSkill::name() const { return "TimerSkill"; }
std::string TimerSkill::description() const { return "TTT"; }
std::string TimerSkill::execute(json j) {
  try {
    std::string type = j.value("type", "");
    if (type == "timer") {
      int min = j.value("minutes", 0);
      if (min <= 0)
        throw std::runtime_error("Бля времени нету");

      add(j.value("name", "общий"), Clock::now() + std::chrono::minutes(min),
          []() {
            std::cout << "Время вышло!\n";
            pid_t timerPid = spawnMPV("./assets/timer.mp3", 100);
            mpvSetVolume(g_volume / 2);
            waitpid(timerPid, nullptr, 0);
            mpvSetVolume(g_volume);
          });
    } else if (type == "alarm") {
      std::string time = j.value("time", "");
      if (time == "")
        throw std::runtime_error("Бля времени нету");
      auto tp = strToTime(time);
      auto zt = std::chrono::zoned_time{std::chrono::current_zone(), tp};
      std::cout << std::format("День:{:%d %H:%M}", zt) << std::endl;
      add(
          j.value("name", "будильник общий"), nextAlarm(strToTime(time)),
          [] {
            std::cout << "Будильник сработал!\n";
            pid_t timerPid = spawnMPV("./assets/timer.mp3", 100);
            mpvSetVolume(g_volume / 2);
            waitpid(timerPid, nullptr, 0);
            mpvSetVolume(g_volume);
          },
          true, std::chrono::Saturday);
    }
    if (!worker.joinable()) {
      worker = std::jthread(&TimerSkill::start, this);
    }
  } catch (const std::exception &e) {
    std::cout << "Ошибка таймера: " << e.what() << "\n\n";
  }
  return "";
}

void TimerSkill::add(std::string name, TimePoint when,
                     std::function<void()> task, bool period,
                     std::optional<std::chrono::weekday> periodDay) {
  std::lock_guard<std::mutex> lg(mtx);
  if (periodDay.has_value()) {
    using namespace std::chrono;
    // TODO:: добавить переодичность
  }
  timers.insert({when, name, task, period, periodDay});
  cv.notify_all();
}
void TimerSkill::add(Timer t) {
  std::lock_guard<std::mutex> lg(mtx);
  timers.insert(std::move(t));
}

void TimerSkill::start() {
  std::unique_lock<std::mutex> lock(mtx);
  while (!stopped) {
    if (timers.empty()) {
      cv.wait(lock, [&] { return stopped || !timers.empty(); });
    }
    if (stopped)
      return;
    cv.wait_until(lock, timers.begin()->when,
                  [&] { return timers.begin()->when <= Clock::now(); });
    if (stopped)
      return;
    auto t = timers.extract(timers.begin());
    if (t.value().period && t.value().periodDay.has_value()) {
      t.value().when += std::chrono::weeks{1};
      add(t.value());
    }
    lock.unlock();
    t.value().task();
    lock.lock();
  }
}

void TimerSkill::stop() {
  {
    std::lock_guard<std::mutex> lg(mtx);
    stopped = true;
  }
  cv.notify_all();
}
