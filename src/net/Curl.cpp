#include "net/Curl.h"
#include <chrono>
#include <cstddef>
#include <cstring>
#include <curl/curl.h>
#include <curl/easy.h>
#include <mutex>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

static size_t BodyRead(void *data, size_t size, size_t nmemb, void *userdata) {
  auto *cb = static_cast<Curl::BodyCallback *>(userdata);
  return (*cb)(static_cast<const char *>(data), size * nmemb);
}

Curl::Curl(const std::string &url) {
  this->url = url;
  curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Ошибка CURL");
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, BodyRead);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
}

void Curl::addHeaders(const std::string &headers) {
  this->headers = curl_slist_append(this->headers, headers.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, this->headers);
}

void Curl::post(const json &body, const BodyCallback &onBody) {
  char errbuf[CURL_ERROR_SIZE];
  std::string strBody = body.dump();
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &onBody);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strBody.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strBody.size());

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    size_t len = strlen(errbuf);
    if (len) {
      throw std::runtime_error(errbuf);
    } else {
      throw std::runtime_error(std::to_string(res));
    }
  }
}
void Curl::get(const std::string &param, const BodyCallback &onBody,
               bool noCache, unsigned int ttl) {

  if (!noCache) {
    auto c = getCached(url + param);
    if (c.has_value()) {
      onBody(c->data(), c->size());
      return;
    }
  }
  std::string cacheBuffer;
  BodyCallback wrapper = [&](const char *data, size_t len) {
    onBody(data, len);
    if (!noCache)
      cacheBuffer.append(data, len);
    return len;
  };

  char errbuf[CURL_ERROR_SIZE] = {};
  curl_easy_setopt(curl, CURLOPT_POST, 0L);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &wrapper);
  curl_easy_setopt(curl, CURLOPT_URL, (url + param).c_str());
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

  CURLcode res = curl_easy_perform(curl);
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  if (res != CURLE_OK) {
    size_t len = strlen(errbuf);
    if (len) {
      throw std::runtime_error(errbuf);
    } else {
      throw std::runtime_error(std::to_string(res));
    }
  }
  if (!noCache) {
    putCached(url + param, cacheBuffer, ttl);
  }
}

std::optional<std::string> Curl::getCached(const std::string &request) {
  std::lock_guard<std::mutex> lg(mtx);
  auto it = cache.find(request);
  if (it != cache.end()) {
    if (std::chrono::steady_clock::now() < it->second.expiry) {
      spdlog::info("Curl: использование кэша: {}", it->second.response);
      return it->second.response;
    } else {
      cache.erase(it);
    }
  }
  return std::nullopt;
}
void Curl::putCached(const std::string &request, const std::string &response,
                     unsigned int ttl) {
  std::lock_guard<std::mutex> lg(mtx);
  cache[request] = {response, std::chrono::steady_clock::now() +
                                  std::chrono::seconds(ttl)};
}
Curl::~Curl() {
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
}
