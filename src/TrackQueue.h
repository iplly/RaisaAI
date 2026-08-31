#pragma once
#include <deque>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>

struct TrackQueue {
  std::deque<struct Track> primary;
  std::deque<struct Track> second;
  enum class Queue { Primary, Second };

  std::optional<Track> popFront();
  void pushPrimary(const Track &t);
  void pushSecond(const Track &t);
  void set(std::deque<Track> &&q);
  void insert(const std::deque<Track> &q);
  void clear();
  void clearSecond();
  void clearPrimary();
  void shufflePrimary();
  friend std::ostream &operator<<(std::ostream &os, TrackQueue &q);

private:
  std::mutex mtx;
};
struct Track {
  std::string title;
  std::string artist;
  std::string id;
};
inline std::ostream &operator<<(std::ostream &os, const Track &t) {
  return os << t.title << " -- " << t.artist;
}
inline std::ostream &operator<<(std::ostream &os, TrackQueue &tq) {
  std::lock_guard<std::mutex> lg(tq.mtx);
  os << "Очередь Primary\n";
  for (const auto &t : tq.primary)
    os << " " << t << "\n";
  os << "Очередь Second\n";
  for (const auto &t : tq.second)
    os << " " << t << "\n";
  return os;
}
