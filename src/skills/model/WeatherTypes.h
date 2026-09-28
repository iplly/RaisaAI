#pragma once
#include <nlohmann/detail/macro_scope.hpp>
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

class WeatherOWMResponse {
  struct Coord {
    double lon = 0.0;
    double lat = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Coord, lon, lat)
  };

  struct Weather {
    int id = 0;
    std::string main;
    std::string description;
    std::string icon;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Weather, id, main, description,
                                                icon)
  };

  struct Main {
    double temp = 0.0;
    double feels_like = 0.0;
    double temp_min = 0.0;
    double temp_max = 0.0;
    int pressure = 0;
    int humidity = 0;
    int sea_level = 0;
    int grnd_level = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Main, temp, feels_like,
                                                temp_min, temp_max, pressure,
                                                humidity, sea_level, grnd_level)
  };

  struct Wind {
    double speed = 0.0;
    int deg = 0;
    double gust = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Wind, speed, deg, gust)
  };

  struct Clouds {
    int all = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Clouds, all)
  };

  struct Sys {
    int type = 0;
    int id = 0;
    std::string country;
    long long sunrise = 0;
    long long sunset = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Sys, type, id, country, sunrise,
                                                sunset)
  };

  struct Precipitation {
    double h1 = 0.0;
    double h3 = 0.0;

    friend void to_json(nlohmann::json &j, const Precipitation &p) {
      j = nlohmann::json::object();
      if (p.h1 != 0.0)
        j["1h"] = p.h1;
      if (p.h3 != 0.0)
        j["3h"] = p.h3;
    }
    friend void from_json(const nlohmann::json &j, Precipitation &p) {
      if (j.contains("1h"))
        p.h1 = j.at("1h").get<double>();
      if (j.contains("3h"))
        p.h3 = j.at("3h").get<double>();
    }
  };

public:
  Coord coord;
  std::vector<Weather> weather;
  std::string base;
  Main main;
  int visibility = 0;
  Wind wind;
  Precipitation rain;
  Precipitation snow;
  Clouds clouds;
  long long dt = 0;
  Sys sys;
  int timezone = 0;
  int id = 0;
  std::string name;
  int cod = 0;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WeatherOWMResponse, coord,
                                              weather, base, main, visibility,
                                              wind, rain, snow, clouds, dt, sys,
                                              timezone, id, name, cod)
};
