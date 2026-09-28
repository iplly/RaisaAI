#include "core/Text.h"
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

std::string toLowerUtf8(const std::string &s) {
  std::string o = s;
  for (size_t i = 0; i < o.size(); i++) {
    unsigned char c = o[i];
    if (c < 0x80) {
      if (c >= 'A' && c <= 'Z')
        o[i] = c + 32;
      continue;
    }
    unsigned char nxt = (i + 1 < o.size()) ? o[i + 1] : 0;
    if (c == 0xD0 && nxt >= 0x90 && nxt <= 0x9F)
      o[i + 1] = nxt + 0x20; // А-П
    else if (c == 0xD0 && nxt >= 0xA0 && nxt <= 0xAF) {
      o[i] = 0xD1;
      o[i + 1] = nxt - 0x20;
    } // Р-Я
    else if (c == 0xD0 && nxt >= 0x80 && nxt <= 0x8F) {
      o[i] = 0xD1;
      o[i + 1] = nxt + 0x10;
    } // Ѐ-Џ
  }
  return o;
}

static std::vector<std::string> split(const std::string &s) {
  std::vector<std::string> tokens;
  std::istringstream iss(s);
  std::string token;
  while (iss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

int wordsToNumber(std::string w) {
  static const std::map<std::string, int> UNITS = {
      {"один", 1}, {"одна", 1},   {"два", 2},   {"две", 2},
      {"три", 3},  {"четыре", 4}, {"пять", 5},  {"шесть", 6},
      {"семь", 7}, {"восемь", 8}, {"девять", 9}};
  static const std::map<std::string, int> TEENS = {
      {"десять", 10},      {"одиннадцать", 11},  {"двенадцать", 12},
      {"тринадцать", 13},  {"четырнадцать", 14}, {"пятнадцать", 15},
      {"шестнадцать", 16}, {"семнадцать", 17},   {"восемнадцать", 18},
      {"девятнадцать", 19}};
  static const std::map<std::string, int> TENS = {
      {"двадцать", 20},    {"тридцать", 30},   {"сорок", 40},
      {"пятьдесят", 50},   {"шестьдесят", 60}, {"семьдесят", 70},
      {"восемьдесят", 80}, {"девяносто", 90}};
  static const std::map<std::string, int> HUNDREDS = {
      {"сто", 100},       {"двести", 200},    {"триста", 300},
      {"четыреста", 400}, {"пятьсот", 500},   {"шестьсот", 600},
      {"семьсот", 700},   {"восемьсот", 800}, {"девятьсот", 900}};
  static const std::map<std::string, long long> SCALES = {
      {"тысяча", 1000}, {"тысячи", 1000}, {"тысяч", 1000}};

  auto tokens = split(w);
  int total = 0;
  for (const auto &token : tokens) {
    auto itUnit = UNITS.find(token);
    auto itTeen = TEENS.find(token);
    auto itTen = TENS.find(token);
    auto itHundred = HUNDREDS.find(token);
    if (itUnit != UNITS.end()) {
      total += itUnit->second;
    } else if (itTeen != TEENS.end()) {
      total += itTeen->second;
    } else if (itTen != TENS.end()) {
      total += itTen->second;
    } else if (itHundred != HUNDREDS.end()) {
      total += itHundred->second;
    } else {
      auto itScale = SCALES.find(token);
      if (itScale != SCALES.end()) {
        int scale = itScale->second;
        if (total == 0)
          total = 1;
        total *= scale;
      }
    }
  }
  return total;
}
std::string numbertowords(int d) {
  std::string sign = d < 0 ? d = -d, "минус " : "";
  std::string out = "";

  static std::vector<std::string> units = {"",       "один",  "два",   "три",
                                           "четыре", "пять",  "шесть", "семь",
                                           "восемь", "девять"};
  static std::vector<std::string> units_f = {"",       "одна",  "две",   "три",
                                             "четыре", "пять",  "шесть", "семь",
                                             "восемь", "девять"};
  static std::vector<std::string> teens = {
      "десять",       "одиннадцать", "двенадцать",  "тринадцать",
      "четырнадцать", "пятнадцать",  "шестнадцать", "семнадцать",
      "восемнадцать", "девятнадцать"};
  static std::vector<std::string> tens = {
      "",          "",           "двадцать",  "тридцать",    "сорок",
      "пятьдесят", "шестьдесят", "семьдесят", "восемьдесят", "девяносто"};
  static std::vector<std::string> hundreds = {
      "",        "сто",      "двести",  "триста",    "четыреста",
      "пятьсот", "шестьсот", "семьсот", "восемьсот", "девятьсот"};
  static std::vector<std::string> scales = {"", "тысяча", "тысячи", "тысяч"};

  if (d == 0)
    return "ноль";

  int u = d % 10;
  int t = ((d - u) % 100) / 10;
  int h = ((d - t * 10) % 1000) / 100;
  int s = d / 1000;
  int su = s % 10;
  int st = ((s - su) % 100) / 10;
  int sh = ((s - st * 10) % 1000) / 100;
  int s10 = s % 10;
  int s100 = s % 100;

  out += hundreds[sh] + (sh ? " " : "");
  if (st == 1) {
    out += teens[su] + " ";
  } else {
    out += tens[st] + (st ? " " : "");
    out += units_f[su] + (su ? " " : "");
  }
  if (s == 0)
    out += scales[0];
  else if (s100 >= 11 && s100 <= 14)
    out += scales[3];
  else if (s10 == 1)
    out += scales[1];
  else if (s10 >= 2 && s10 <= 4)
    out += scales[2];
  else
    out += scales[3];

  out += (s && (h || t || u) ? " " : "");

  out += hundreds[h] + ((t && h) || (h && u) ? " " : "");
  if (t == 1) {
    out += teens[u];
  } else {
    out += tens[t] + (u && t ? " " : "");
    out += units[u];
  }

  return sign + out;
}

std::string toPrepositional(std::string city) {
  static const std::map<std::string, std::string> ex = {
      {"Москва", "Москве"},
      {"Пермь", "Перми"},
      {"Питер", "Питере"},
      {"Санкт-Петербург", "Санкт-Петербурге"},
      {"Сочи", "Сочи"},
      {"Токио", "Токио"},
      {"Алматы", "Алматы"},
      {"Баку", "Баку"},
      {"Грозный", "Грозном"},
      {"Орёл", "Орле"},
      {"Ростов-на-Дону", "Ростове-на-Дону"},
      {"Нижний Новгород", "Нижнем Новгороде"},
      {"Йошкар-Ола", "Йошкар-Оле"},
  };
  if (auto it = ex.find(city); it != ex.end())
    return it->second;
  return city;
}

std::string urlEncode(const std::string &s) {
  static const char *hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size() * 3);
  for (unsigned char c : s) { // unsigned — иначе сравнения сломаются
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~')
      out += c; // алифанумерика и -._~ — как есть
    else {
      out += '%';
      out += hex[c >> 4]; // старшая половина байта → %XX
      out += hex[c & 0xF];
    }
  }
  return out;
}
