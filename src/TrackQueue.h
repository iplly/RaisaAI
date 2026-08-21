
#include <deque>
#include <mutex>
#include <optional>
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

private:
  std::mutex mtx;
};
struct Track {
  std::string title;
  std::string artist;
  std::string id;
};
