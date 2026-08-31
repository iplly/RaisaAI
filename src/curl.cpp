#include "curl.h"
#include <cstddef>
#include <curl/curl.h>
#include <curl/easy.h>
#include <string>

using json = nlohmann::json;

static size_t BodyRead(void *data, size_t size, size_t nmemb, void *userdata) {
  auto *cb = static_cast<Curl::BodyCallback *>(userdata);
  return (*cb)(static_cast<const char *>(data), size * nmemb);
}

Curl::Curl(std::string url) {
  this->url = url;
  curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("Ошибка CURL");
  }
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, BodyRead);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
}

void Curl::addHeaders(const std::string &headers) {
  this->headers = curl_slist_append(this->headers, headers.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, this->headers);
}

CURLcode Curl::post(const json &body, const BodyCallback &onBody) {
  std::string strBody = body.dump();
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &onBody);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, strBody.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strBody.size());

  return curl_easy_perform(curl);
}
CURLcode Curl::get(std::string param, const BodyCallback &onBody) {
  curl_easy_setopt(curl, CURLOPT_POST, 0L);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &onBody);
  curl_easy_setopt(curl, CURLOPT_URL, (url + param).c_str());

  CURLcode res = curl_easy_perform(curl);

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  return res;
}
Curl::~Curl() {
  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
}
