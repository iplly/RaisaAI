#pragma once
#include <atomic>
#include <nlohmann/detail/macro_scope.hpp>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
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

namespace nlohmann {
template <typename T> struct adl_serializer<std::optional<T>> {
  static void to_json(json &j, const std::optional<T> &opt) {
    if (opt.has_value()) {
      j = *opt;
    } else {
      j = nullptr;
    }
  }

  static void from_json(const json &j, std::optional<T> &opt) {
    if (j.is_null()) {
      opt = std::nullopt;
    } else {
      opt = j.get<T>();
    }
  }
};
} // namespace nlohmann

class LLMTool {
  class Function {
    class Parameters {
      struct prop {
        std::string type;
        std::string description;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(prop, type, description)
      };

      const std::string type = "object";

    public:
      std::vector<std::string> required;
      std::unordered_map<std::string, prop> properties;

      NLOHMANN_DEFINE_TYPE_INTRUSIVE(Parameters, type, required, properties)
    };

  public:
    std::string name;
    std::string description;
    Parameters parameters;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Function, name, description, parameters)
  };

public:
  Function function;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(LLMTool, type, function)
private:
  const std::string type = "function";
};

struct LLMMessage {
  std::string role;
  std::string content;
  std::optional<std::string> tool_call_id = std::nullopt;
  std::optional<std::vector<json>> tool_calls = std::nullopt;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(LLMMessage, role, content, tool_call_id,
                                 tool_calls)
};

struct LLMRequest {
  std::string model;
  bool stream;
  int keep_alive;
  std::vector<LLMMessage> messages;
  std::vector<LLMTool> tools;

  NLOHMANN_DEFINE_TYPE_INTRUSIVE(LLMRequest, model, stream, keep_alive,
                                 messages, tools)
};
