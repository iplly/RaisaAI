#pragma once
#include <map>
#include <string>

class Config {
public:
  static Config &instance();
  void load(const std::string &path = "./raisa.conf");
  const std::string &get(const std::string &key,
                         const std::string &def = "") const;

  Config(const Config &) = delete;
  Config &operator=(const Config &) = delete;

private:
  std::map<std::string, std::string> kv;
  Config() = default;
  ~Config() = default;
};
