#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct WeatherCurrentUnits {
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
  NLOHMANN_DEFINE_TYPE_INTRUSIVE(WeatherCurrentUnits, time, interval,
                                 temperature_2m, relative_humidity_2m,
                                 apparent_temperature, is_day, precipitation,
                                 rain, weather_code, cloud_cover, pressure_msl,
                                 wind_speed_10m, wind_direction_10m,
                                 wind_gusts_10m)
};

struct WeatherCurrent {
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

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(WeatherCurrent, time, interval, temperature_2m,
                                 relative_humidity_2m, apparent_temperature,
                                 is_day, precipitation, rain, weather_code,
                                 cloud_cover, pressure_msl, wind_speed_10m,
                                 wind_direction_10m, wind_gusts_10m)
};

struct WeatherDailyUnits {
  std::string time;
  std::string weather_code;
  std::string temperature_2m_max;
  std::string temperature_2m_min;
  std::string precipitation_probability_max;
  std::string sunrise;
  std::string sunset;
  std::string uv_index_max;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(WeatherDailyUnits, time, weather_code,
                                 temperature_2m_max, temperature_2m_min,
                                 precipitation_probability_max, sunrise, sunset,
                                 uv_index_max)
};

struct WeatherDaily {
  std::vector<std::string> time;
  std::vector<int> weather_code;
  std::vector<double> temperature_2m_max;
  std::vector<double> temperature_2m_min;
  std::vector<int> precipitation_probability_max;
  std::vector<std::string> sunrise;
  std::vector<std::string> sunset;
  std::vector<double> uv_index_max;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(WeatherDaily, time, weather_code,
                                 temperature_2m_max, temperature_2m_min,
                                 precipitation_probability_max, sunrise, sunset,
                                 uv_index_max)
};

struct WeatherResponse {
  double latitude;
  double longitude;
  double generationtime_ms;
  int utc_offset_seconds;
  std::string timezone;
  std::string timezone_abbreviation;
  double elevation;
  WeatherCurrentUnits current_units;
  WeatherCurrent current;
  WeatherDailyUnits daily_units;
  WeatherDaily daily;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(WeatherResponse, latitude, longitude,
                                 generationtime_ms, utc_offset_seconds,
                                 timezone, timezone_abbreviation, elevation,
                                 current_units, current, daily_units, daily)
};