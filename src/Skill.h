
#pragma once
#include "SkillTool.h"
#include "TrackQueue.h"
#include "VoiceController.h"
#include "curl.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

struct StreamBuffer {
  std::string part;
  std::string full;
};

class Skill {
protected:
  std::jthread worker;
  std::atomic<bool> busy{false};
  std::atomic<bool> stopFlag{false};

public:
  virtual std::string name() const = 0;
  virtual std::string description() const = 0;
  virtual std::string execute(json) = 0;
  virtual void stop() {};

  virtual ~Skill() = default;
};

class WeatherSkill : public Skill {
public:
  std::string name() const override;
  std::string description() const override;
  std::string execute(json) override;
  std::string start(std::string, bool);
};

class LlmSkill : public Skill {
  StreamBuffer stream;
  void start(std::string);

public:
  std::string name() const override;
  std::string description() const override;
  std::string execute(json) override;
  void stop() override;
  bool running() const;
};

class VKMusicSkill : public Skill {
  struct mixType {
    std::string vibes = "";
    std::string recognitions = "";
    std::string langs = "";
  };

  std::atomic<pid_t> childPid{-1};
  struct TrackQueue trackQueue;
  std::atomic<bool> mixStatus{false};
  std::mutex vkMtx;
  void start(std::string, std::string, mixType);
  void player(mixType &);
  void shuffle(std::deque<Track> &queue);
  void addTo(TrackQueue::Queue, std::string);
  std::deque<Track> vk(std::string cmd, std::string args = "");
  std::deque<Track> search(std::string);
  std::deque<Track> mix(mixType mt = {"\"\"", "\"\"", "\"\""});
  std::deque<Track> my();
  std::deque<Track> similar(std::string);
  std::deque<Track> playlist(std::string);
  std::string getStream(std::string);
  std::string extractTrackName(std::string,
                               std::function<json()> = vkmusicTool);

public:
  friend void debugConsole();
  friend void debugInit(VoiceController &vc);
  std::string name() const override;
  std::string description() const override;
  std::string execute(json) override;
  void stop() override;
  void next();
  void add(std::string);
  void addToEnd(std::string);
  void shuffleQueue();
  void clearSecond();
  void clearPrimary();
  bool running() const;
};

class TimerSkill : public Skill {
  using Clock = std::chrono::system_clock;
  using TimePoint = Clock::time_point;
  std::mutex mtx;
  std::condition_variable cv;
  std::atomic<bool> stopped{false};

  struct Timer {
    TimePoint when;
    std::string name;
    std::function<void()> task;
    bool period{false};
    std::optional<std::chrono::weekday> periodDay;
    bool operator<(const Timer &o) const { return when < o.when; }
  };
  std::multiset<Timer> timers;

public:
  friend void debugConsole();
  friend void debugInit(VoiceController &vc);
  std::string name() const override;
  std::string description() const override;
  std::string execute(json) override;
  void start();
  void add(std::string, TimePoint, std::function<void()>, bool period = false,
           std::optional<std::chrono::weekday> periodDay = {});
  void add(Timer);
  void stop() override;
};

struct Skills {
  std::unique_ptr<WeatherSkill> weather;
  // std::unique_ptr<YTMusicSkill> ytmusic;
  std::unique_ptr<VKMusicSkill> vkmusic;
  std::unique_ptr<LlmSkill> llmskill;
  std::unique_ptr<TimerSkill> timerskill;
};

struct SkillReg {
  std::string name;
  std::function<json()> tool;
  std::vector<std::string> keywords;
  std::function<void(json)> execute;
};

extern struct Skills g_skills;
extern json qwen1_7Data;
extern std::vector<SkillReg> g_registry;
void skill_init();
void dispatch(const std::string &name, const json &args,
              const std::string &full);
json SkillChoser(const std::string message);
std::string urlEncode(const std::string &s);
