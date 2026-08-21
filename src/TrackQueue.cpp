#include "TrackQueue.h"
#include <algorithm>
#include <deque>
#include <mutex>
#include <optional>
#include <random>
#include <utility>

std::optional<Track> TrackQueue::popFront() {
  std::lock_guard<std::mutex> lg(mtx);
  if (primary.empty() && second.empty())
    return std::nullopt;
  if (!second.empty()) {
    Track t = std::move(second.front());
    second.pop_front();
    return t;
  }
  Track t = std::move(primary.front());
  primary.pop_front();
  return t;
}
void TrackQueue::pushPrimary(const Track &t) {
  std::lock_guard<std::mutex> lg(mtx);
  primary.push_back(t);
}

void TrackQueue::pushSecond(const Track &t) {
  std::lock_guard<std::mutex> lg(mtx);
  second.push_back(t);
}
void TrackQueue::set(std::deque<Track> &&q) {
  std::lock_guard<std::mutex> lg(mtx);
  primary = std::move(q);
  second.clear();
}

void TrackQueue::clear() {
  std::lock_guard<std::mutex> lg(mtx);
  primary.clear();
  second.clear();
}
void TrackQueue::clearSecond() {
  std::lock_guard<std::mutex> lg(mtx);
  second.clear();
}
void TrackQueue::clearPrimary() {
  std::lock_guard<std::mutex> lg(mtx);
  primary.clear();
}
void TrackQueue::insert(const std::deque<Track> &qs) { // prefetch
  std::lock_guard lk(mtx);
  primary.insert(primary.begin(), qs.begin(), qs.end());
}
void TrackQueue::shufflePrimary() {
  std::lock_guard lk(mtx);
  std::shuffle(primary.begin(), primary.end(),
               std::mt19937{std::random_device{}()});
}
