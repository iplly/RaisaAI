#pragma once
#include "core/Config.h"
#include "daemon/VoiceController.h"
#include "playback/TrackQueue.h"
#include "skills/ToolDefs.h"
#include "skills/model/LmTypes.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <sys/types.h>
#include <thread>
#include <vector>

using json = nlohmann::json;

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
  const std::function<json()> tool;
  virtual std::string execute(json) = 0;
  virtual void stop() {};

  virtual ~Skill() = default;
};

class WeatherSkill : public Skill {
public:
  std::string name() const override;
  std::string description() const override;
  const std::function<json()> tool = weatherTool;
  std::string execute(json) override;
  std::string start(std::string, bool);
};

class LLMSkill : public Skill {
  StreamBuffer stream;
  LLMRequest context = {.model = Config::instance().get("LLM_MODEL"),
                        .stream = true,
                        .keep_alive = -1,
                        .options = {},
                        .messages = {},
                        .tools = {}};
  void start(std::string);
  std::vector<LLMMessage> ToolChoser(const json &);
  std::optional<std::string> _lastResponse = std::nullopt;

public:
  std::string name() const override;
  std::string description() const override;
  std::string execute(json) override;
  std::optional<std::string> lastResponse();
  void lastResponseReset();
  std::string systemStatusPrompt();
  void stop() override;
  LLMSkill();
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
  Track _nowPlaying = {"", "", ""};
  std::atomic<bool> mixStatus{false};
  std::atomic<bool> searchStatus{false};
  std::mutex vkMtx;
  void start(const std::string &, const std::string &, mixType);
  void player(mixType &);
  void shuffle(std::deque<Track> &queue);
  void addTo(TrackQueue::Queue, const std::string &query);
  std::deque<Track> vk(const std::string &cmd, const std::string &args = "");
  std::deque<Track> search(const std::string &query, unsigned int count = 1);
  std::deque<Track> mix(const mixType &mt = {"\"\"", "\"\"", "\"\""});
  std::deque<Track> my();
  std::deque<Track> similar(const std::string &);
  std::deque<Track> playlist(const std::string &);
  std::string getStream(const std::string &);
  std::string extractTrackName(const std::string &query,
                               const std::function<json()> &tool = vkmusicTool);

public:
  friend void debugConsole();
  friend void debugInit(VoiceController &vc);
  std::string name() const override;
  std::string description() const override;
  const std::function<json()> tool = vkmusicTool;
  std::string execute(json) override;
  Track nowPlaying() const;
  void stop() override;
  void next();
  void add(const std::string &);
  void addToEnd(const std::string &);
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
  const std::function<json()> tool = timerTool;
  std::string execute(json) override;
  void start();
  void add(std::string, TimePoint, std::function<void()>, bool period = false,
           std::optional<std::chrono::weekday> periodDay = {});
  void add(Timer);
  void stop() override;
};
