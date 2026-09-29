#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

class TTSClient {
  struct TTSJob {
    std::string text;
    std::string instructions = "";
  };
  std::mutex mtx;
  std::condition_variable cv;
  std::deque<TTSJob> queue;
  std::atomic<bool> stopFlag{false};
  std::atomic<bool> active{false};
  std::atomic<pid_t> mpvPid;
  std::jthread worker;
  void run();
  void generate(const TTSJob &task);

public:
  TTSClient();
  void speak(const std::string &text, const std::string &instructions = "");
  void stop();
  bool speaking() const;
};
