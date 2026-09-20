#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>

using json = nlohmann::json;

json weatherTool();
json ytmusicTool();
json ytmusicAddTool();
json vkmusicTool();
json vkmusicAddTool();
json vkmusicPlaylistTool();
json timerTool();

struct retOpt {
  std::unordered_map<std::string, std::string> vibes = {
      {"happy", "весёлый"},  {"sad", "грустный"},       {"active", "активный"},
      {"calm", "спокойный"}, {"love", "романтический"}, {"", ""}};
  std::unordered_map<std::string, std::string> recognitions = {
      {"known", "знакомыми"},
      {"unknown", "незнакомыми"},
      {"fresh", "новинками"},
      {"", ""}};
  std::unordered_map<std::string, std::string> langs = {
      {"ru", "на русском"},
      {"international", "иностранные"},
      {"instrumental", "инструментальные"},
      {"", ""}};
};
