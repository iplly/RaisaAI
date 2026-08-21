#pragma once
#include <cstddef>
#include <curl/curl.h>
#include <functional>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

using json = nlohmann::json;

class Curl {
  CURL *curl;
  std::string url;
  struct curl_slist *headers = nullptr;

public:
  using BodyCallback = std::function<std::size_t(const char *, std::size_t)>;
  Curl(std::string url);
  void addHeaders(const std::string &headers);
  CURLcode post(const json &body, const BodyCallback &onBody);
  CURLcode get(std::string param, const BodyCallback &onBody);
  ~Curl();
};
