#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class TTSClient {
  std::mutex mtx;
  std::condition_variable cv;
  std::deque<std::string> queue;
  std::atomic<bool> stopFlag{false};
  std::atomic<bool> active{false};
  std::atomic<pid_t> mpvPid;
  std::jthread worker;
  void run();
  bool generate(const std::string &text);

public:
  TTSClient();
  void speak(const std::string &text);
  void stop();
  bool speaking() const;
};
