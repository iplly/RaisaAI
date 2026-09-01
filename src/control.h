#pragma once
#include <atomic>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/types.h>
#include <vector>

using json = nlohmann::json;

extern std::atomic<pid_t> g_vkMpvPid;
extern std::atomic<uint8_t> g_volume;
extern std::atomic<uint8_t> g_actual;
extern std::atomic<bool> g_pause;
extern const std::string dirControl;

std::string exec(std::string args);
void mpvSetVolume_(char v, pid_t pid = g_vkMpvPid);
void mpvSetVolume(char v, pid_t pid = g_vkMpvPid);
void setVolume(uint8_t v);
void mpvTogglePause(pid_t pid = g_vkMpvPid);
int wordsToNumber(std::string);
std::string toLowerUtf8(const std::string &s);
std::string toPrepositional(std::string city);
pid_t spawnMPV(const std::string &url, uint8_t volume = g_volume);

struct CurrentUnits {
  std::string time;
  std::string interval;
  std::string temperature_2m;
  std::string relative_humidity_2m;
  std::string apparent_temperature;
  std::string is_day;
  std::string precipitation;
  std::string rain;
  std::string weather_code;
  std::string cloud_cover;
  std::string pressure_msl;
  std::string wind_speed_10m;
  std::string wind_direction_10m;
  std::string wind_gusts_10m;

  // Макрос ВНУТРИ – все поля перечисляем через запятую
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(CurrentUnits, time, interval, temperature_2m,
                                 relative_humidity_2m, apparent_temperature,
                                 is_day, precipitation, rain, weather_code,
                                 cloud_cover, pressure_msl, wind_speed_10m,
                                 wind_direction_10m, wind_gusts_10m)
};

struct Current {
  std::string time;
  int interval;
  double temperature_2m;
  int relative_humidity_2m;
  double apparent_temperature;
  int is_day;
  double precipitation;
  double rain;
  int weather_code;
  int cloud_cover;
  double pressure_msl;
  double wind_speed_10m;
  int wind_direction_10m;
  double wind_gusts_10m;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Current, time, interval, temperature_2m,
                                 relative_humidity_2m, apparent_temperature,
                                 is_day, precipitation, rain, weather_code,
                                 cloud_cover, pressure_msl, wind_speed_10m,
                                 wind_direction_10m, wind_gusts_10m)
};

struct DailyUnits {
  std::string time;
  std::string weather_code;
  std::string temperature_2m_max;
  std::string temperature_2m_min;
  std::string precipitation_probability_max;
  std::string sunrise;
  std::string sunset;
  std::string uv_index_max;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(DailyUnits, time, weather_code,
                                 temperature_2m_max, temperature_2m_min,
                                 precipitation_probability_max, sunrise, sunset,
                                 uv_index_max)
};

struct Daily {
  std::vector<std::string> time;
  std::vector<int> weather_code;
  std::vector<double> temperature_2m_max;
  std::vector<double> temperature_2m_min;
  std::vector<int> precipitation_probability_max;
  std::vector<std::string> sunrise;
  std::vector<std::string> sunset;
  std::vector<double> uv_index_max;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Daily, time, weather_code, temperature_2m_max,
                                 temperature_2m_min,
                                 precipitation_probability_max, sunrise, sunset,
                                 uv_index_max)
};

struct Response {
  double latitude;
  double longitude;
  double generationtime_ms;
  int utc_offset_seconds;
  std::string timezone;
  std::string timezone_abbreviation;
  double elevation;
  CurrentUnits current_units;
  Current current;
  DailyUnits daily_units;
  Daily daily;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(Response, latitude, longitude,
                                 generationtime_ms, utc_offset_seconds,
                                 timezone, timezone_abbreviation, elevation,
                                 current_units, current, daily_units, daily)
};
