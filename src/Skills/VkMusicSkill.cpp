#include "../Skill.h"
#include "Ollama.h"
#include "control.h"
#include <csignal>
#include <future>
#include <iostream>
#include <random>
#include <string>
#include <sys/wait.h>

std::string VKMusicSkill::name() const { return "VKMusicSkill"; }
std::string VKMusicSkill::description() const { return "VKMusicSkill"; }
bool VKMusicSkill::running() const { return busy; }
std::string VKMusicSkill::execute(json j) {
  mixType m;
  std::string track = j.value("track", "");
  std::string type = j.value("type", "search");
  if (type == "mix") {
    m.vibes = j.value("vibes", "");
    m.recognitions = j.value("recognitions", "");
    m.langs = j.value("langs", "");
  }
  std::cout << "VKMusicSkill execute: " << j << "\n\n";
  if (busy) {
    stop();
  }
  if (worker.joinable())
    worker.join();
  busy = true;
  stopFlag = false;
  worker = std::jthread(&VKMusicSkill::start, this, track, type, m);
  return "";
}

void VKMusicSkill::stop() {
  std::cout << "Останавливаюсь\n";
  stopFlag = true;
  if (childPid > 0) {
    kill(childPid, SIGTERM);
    waitpid(childPid, nullptr, 0);
    childPid = -1;
  }
  busy = false;
  mixStatus = false;
}
void VKMusicSkill::next() {
  std::cout << "Следующий трек\n";
  if (childPid > 0)
    kill(childPid, SIGTERM);
}

std::deque<Track> VKMusicSkill::vk(std::string cmd, std::string args) {
  try {
    std::lock_guard<std::mutex> lg(vkMtx);
    std::deque<Track> queueTracks;
    std::cout << "python vk.py " + cmd + " " + args + "\n\n";
    json search_result =
        json::parse(exec("python src/vkmusic/vk.py " + cmd + " " +
                         (args.empty() ? "" : "\"" + args + "\"")));
    if (!search_result["ok"])
      throw std::runtime_error("Пустой результат ");
    for (const auto &track : search_result["tracks"])
      queueTracks.push_back({track["title"], track["artist"], track["id"]});

    return queueTracks;
  } catch (const std::exception &e) {
    std::cout << "Ошибка выполнения vk.py " << e.what() << "\n\n";
    return {};
  }
}

std::deque<Track> VKMusicSkill::search(std::string query) {
  std::deque<Track> queueTracks = vk("search", query);
  if (queueTracks.empty())
    throw std::runtime_error("Ошибка поиска ");
  if (queueTracks.size() > 3)
    queueTracks.resize(3);
  return queueTracks;
}

std::string VKMusicSkill::extractTrackName(std::string query,
                                           std::function<json()> tool) {
  json jsonData = qwen1_7Data;
  Ollama ollama;
  try {
    jsonData["tools"].push_back(tool());
    jsonData["messages"].push_back(
        {{"role", "user"}, {"content", query.c_str()}});

    json trackName = ollama.chat(
        jsonData)["message"]["tool_calls"][0]["function"]["arguments"]["track"];
    return trackName;
  } catch (const std::exception &e) {
    std::cout << "Ошибка извлечения названия трека " << e.what() << "\n\n";
    return "";
  }
}

void VKMusicSkill::add(std::string query) {
  addTo(TrackQueue::Queue::Second, query);
}
void VKMusicSkill::addToEnd(std::string query) {
  addTo(TrackQueue::Queue::Primary, query);
}
void VKMusicSkill::addTo(TrackQueue::Queue queue, std::string query) {
  try {
    json trackName = extractTrackName(query, vkmusicAddTool);

    std::cout << "trackName: " << trackName << "\n\n";
    std::deque<Track> search_result = search(trackName.dump());
    if (search_result.empty())
      throw std::runtime_error("Пустой поиск addTo ");

    if (queue == TrackQueue::Queue::Primary)
      trackQueue.pushPrimary(search_result.front());
    else
      trackQueue.pushSecond(search_result.front());
    std::cout << "Трек добавлен в очередь\n\n";

  } catch (const std::exception &e) {
    std::cout << "Ошибка добавления трека: " << e.what() << "\n\n";
  }
}

void VKMusicSkill::shuffle(std::deque<Track> &queue) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::shuffle(queue.begin(), queue.end(), gen);
}

void VKMusicSkill::shuffleQueue() { trackQueue.shufflePrimary(); }

void VKMusicSkill::start(std::string track, std::string type, mixType mt) {
  std::deque<Track> first;
  trackQueue.clear();
  try {
    if (type == "mix") {
      first = mix(mt);
      mixStatus = true;
    } else if (type == "playlist") {
      first = playlist(track);
      shuffle(first);
    } else if (type == "my") {
      first = my();
    } else {
      first = search(track);
    }
    trackQueue.set(std::move(first));
    player(mt);
  } catch (const std::exception &e) {
    std::cout << "Ошибка запуска музыки" << e.what() << "\n\n";
    busy = false;
  }

  childPid = -1;
  busy = false;
}

void VKMusicSkill::player(mixType &mt) {
  std::future<std::deque<Track>> prefetch;
  std::string currentTrack = "spmsE_9Np6M";

  while (!trackQueue.primary.empty() || !trackQueue.second.empty()) {
    try {
      if (stopFlag)
        return;

      auto [title, artist, id] = trackQueue.popFront().value();
      currentTrack = id;

      if (trackQueue.primary.empty() && !mixStatus)
        prefetch = std::async(std::launch::async, [this, currentTrack] {
          return similar(currentTrack);
        });
      else if (trackQueue.primary.empty() && mixStatus)
        prefetch =
            std::async(std::launch::async, [this, mt] { return mix(mt); });

      std::string stream_url = getStream(id);
      if (stopFlag)
        return;

      std::cout << trackQueue << "\n";

      std::cout << "\nВключаю: " << artist << " -- " << title << "\n\n";
      g_vkMpvPid = spawnMPV(stream_url);
      childPid = g_vkMpvPid.load();
      waitpid(g_vkMpvPid, nullptr, 0);
      if (prefetch.valid() && trackQueue.primary.empty()) {
        std::deque<Track> mixTracks = prefetch.get();
        trackQueue.insert(mixTracks);
        prefetch = {};
      }
    } catch (const std::exception &e) {
      std::cout << "Ошибка проигрывания музыки " << e.what() << "\n\n";
    }
  }
}

std::deque<Track> VKMusicSkill::my() { return vk("my", "2000"); }
std::deque<Track> VKMusicSkill::mix(mixType mt) {
  return vk("mix " + mt.vibes + " " + mt.recognitions + " " + mt.langs);
}
std::deque<Track> VKMusicSkill::similar(std::string id) {
  return vk("similar", id);
}
std::deque<Track> VKMusicSkill::playlist(std::string playlistName) {
  std::map<std::string, int> playlistDict{{"для вас", -21},
                                          {"плейлист недели", -22},
                                          {"новинки", -23},
                                          {"открытия", -24},
                                          {"плейлист дня 1", -25},
                                          {"плейлист дня 2", -26},
                                          {"плейлист дня 3", -27},
                                          {"плейлист дня 4", -28},
                                          {"плейлист дня 5", -29},
                                          {"плейлист в дорогу", -31},
                                          {"грустно", -33},
                                          {"радостно", -34},
                                          {"активно", -35},
                                          {"спокойно", -36},
                                          {"новый год", -41},
                                          {"любовь", -42},
                                          {"электроника", -43},
                                          {"хип-хоп", -44},
                                          {"поп", -45},
                                          {"фолк", -60},
                                          {"утро", -70}};
  try {
    std::deque<Track> queueTracks;
    int playlistId = -21;
    std::string playlistName2 = "none";
    std::cout << "playlistName: " << toLowerUtf8(playlistName) << "\n\n";
    for (const auto &playlist : playlistDict) {
      if (toLowerUtf8(playlistName).find(playlist.first) != std::string::npos) {
        playlistId = playlist.second;
        playlistName2 = playlist.first;
        break;
      }
    }
    return vk("playlist", std::to_string(playlistId));
  } catch (const std::exception &e) {
    std::cout << "Ошибка включения плейлиста " << e.what() << "\n\n";
    return {};
  }
}

void VKMusicSkill::clearSecond() { trackQueue.clearSecond(); }
void VKMusicSkill::clearPrimary() { trackQueue.clearPrimary(); }

std::string VKMusicSkill::getStream(std::string id) {
  json stream_url = json::parse(exec("python src/vkmusic/vk.py stream " + id));
  if (!stream_url.is_object() || stream_url.value("ok", false) == false) {
    std::cout << "Нет стрима\n\n";
    return {};
  }
  std::cout << "stream_url: " << stream_url["url"].get<std::string>() << "\n\n";
  return stream_url["url"];
}
