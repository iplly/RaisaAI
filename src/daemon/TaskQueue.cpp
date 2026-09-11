#include "daemon/TaskQueue.h"
#include <mutex>

void TaskQueue::push(std::string s) {
  std::lock_guard<std::mutex> lg(mtx);
  tasks.push_back(s);
  cv.notify_one();
}
std::string TaskQueue::pop() {
  std::unique_lock<std::mutex> lock(mtx);
  cv.wait(lock, [&] { return !tasks.empty() || stopped; });
  std::string task;
  if (!tasks.empty()) {
    task = tasks.front();
    tasks.pop_front();
  }
  return task;
}
void TaskQueue::stop() {
  stopped = true;
  cv.notify_all();
}