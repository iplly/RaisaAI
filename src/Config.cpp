#include "Config.h"
#include <cstddef>
#include <fstream>
#include <stdexcept>
#include <string>

static std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  size_t end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

Config &Config::instance() {
  static Config instance;
  return instance;
}

void Config::load(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open())
    throw std::runtime_error("Ошибка открытия конфига");
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    size_t pos = line.find('=');
    if (pos == std::string::npos)
      throw std::runtime_error("Пропушено \'=\'");
    std::string key = trim(line.substr(0, pos));
    std::string value = trim(line.substr(pos + 1));
    if (!key.empty() && !value.empty())
      kv[key] = value;
  }
}
const std::string &Config::get(const std::string &key,
                               const std::string &def) const {
  return kv.contains(key) ? kv.at(key) : def;
}
