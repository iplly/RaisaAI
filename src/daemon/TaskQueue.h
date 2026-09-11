#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

struct TaskQueue {
  std::mutex mtx;
  std::condition_variable cv;
  std::deque<std::string> tasks;
  std::atomic<bool> stopped{false};
  void push(std::string);
  std::string pop();
  void stop();
};