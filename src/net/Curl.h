#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <curl/curl.h>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

struct CacheEntry {
  std::string response;
  std::chrono::steady_clock::time_point expiry;
};

class Curl {
  CURL *curl;
  inline static std::mutex mtx;
  std::string url;
  struct curl_slist *headers = nullptr;
  inline static std::unordered_map<std::string, CacheEntry> cache;
  inline static std::atomic<std::chrono::steady_clock::time_point>
      lastCleanup{};
  inline static constexpr auto cleanupInterval = std::chrono::seconds(1800);

public:
  using BodyCallback = std::function<std::size_t(const char *, size_t)>;
  Curl(const std::string &url);
  void addHeaders(const std::string &headers);
  void post(const json &body, const BodyCallback &onBody);
  void get(const std::string &param, const BodyCallback &onBody,
           bool noCache = false, unsigned int ttl = 1800);
  std::optional<std::string> getCached(const std::string &request);
  void putCached(const std::string &request, const std::string &response,
                 unsigned int ttl);
  static void clearExpired();
  Curl(Curl const &) = delete;
  Curl &operator=(Curl const &) = delete;
  Curl(Curl &&) = delete;
  Curl &operator=(Curl &&) = delete;
  ~Curl();
};
