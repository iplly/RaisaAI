#include "../Skill.h"
#include "control.h"
#include <iostream>

static std::string wmo2ru(int code) {
  if (code == 0)
    return "ясно";
  if (code <= 2)
    return "малооблачно";
  if (code == 3)
    return "пасмурно";
  if (code == 45 || code == 48)
    return "туман";
  if (code >= 51 && code <= 57)
    return "морось";
  if (code >= 61 && code <= 67)
    return "дождь";
  if (code >= 71 && code <= 77)
    return "снег";
  if (code >= 80 && code <= 82)
    return "ливень";
  if (code >= 95 && code <= 99)
    return "гроза";
  return "";
}
static std::string windDirectionText(int degrees) {
  const std::vector<std::string> dirs = {"С", "СВ", "В", "ЮВ",
                                         "Ю", "ЮЗ", "З", "СЗ"};
  int idx = static_cast<int>((degrees + 22.5) / 45.0) % 8;
  return dirs[idx];
}
std::string extractTime(const std::string &iso8601) {
  size_t tpos = iso8601.find('T');
  if (tpos != std::string::npos) {
    return iso8601.substr(tpos + 1, 5); // "HH:MM"
  }
  return iso8601;
}

std::string WeatherSkill::name() const { return "WeatherSkill"; }
std::string WeatherSkill::description() const {
  return "Получить текущую погоду в указанном городе. "
         "Используй ТОЛЬКО если пользователь спрашивает про погоду или "
         "температуру.";
}
std::string WeatherSkill::execute(json j) {
  std::string city = (j.contains("city") && !j["city"].is_null())
                         ? j["city"].get<std::string>()
                         : "null";
  bool detal = (j.contains("detals") ? j["detals"].get<bool>() : false);
  std::cout << "city: " << city << "\n\n";
  if (busy)
    return "Ждите";
  if (worker.joinable())
    worker.join();
  busy = true;
  stopFlag = false;
  worker = std::jthread(&WeatherSkill::start, this, city, detal);
  return "";
}
std::string WeatherSkill::start(std::string city, bool detal) {
  try {
    Curl curlIP("https://free.freeipapi.com/api/json/");
    Curl curlGeo("https://geocoding-api.open-meteo.com/v1/");
    Curl curlMeteo("https://api.open-meteo.com/v1/");
    std::string result;
    if (city == "null") {
      curlIP.get("", [&](const char *data, size_t len) -> size_t {
        result.append(data, len);
        return len;
      });
      json curlJson = json::parse(result);
      city = curlJson["cityName"];
      std::cout << "cityName: " << city << "\n\n";
      result.clear();
    }

    curlGeo.get("search?name=" + urlEncode(city) + "&count=1&language=ru",
                [&](const char *data, size_t len) -> size_t {
                  result.append(data, len);
                  return len;
                });
    json geoJson = json::parse(result)["results"][0];
    std::cout << geoJson << "\n\n";

    result.clear();
    curlMeteo.get(
        "forecast?latitude=" +
            std::to_string(geoJson["latitude"].get<double>()) +
            "&longitude=" + std::to_string(geoJson["longitude"].get<double>()) +
            "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
            "is_day,precipitation,rain,weather_code,cloud_cover,pressure_msl,"
            "wind_speed_10m,wind_direction_10m,wind_gusts_10m&daily=weather_"
            "code,temperature_2m_max,temperature_2m_min,precipitation_"
            "probability_max,sunrise,sunset,uv_index_max&timezone=auto&"
            "forecast_days=1",
        [&](const char *data, size_t len) -> size_t {
          result.append(data, len);
          return len;
        });
    Response meteoJson = json::parse(result).get<Response>();
    const auto &cur = meteoJson.current;
    double temp = cur.temperature_2m;
    double feels = cur.apparent_temperature;
    int humidity = cur.relative_humidity_2m;
    double wind_speed = cur.wind_speed_10m;
    int wind_dir = cur.wind_direction_10m;
    double pressure = cur.pressure_msl;
    std::string weather = wmo2ru(cur.weather_code);

    const auto &day = meteoJson.daily;
    double t_min = day.temperature_2m_min[0];
    double t_max = day.temperature_2m_max[0];
    int precip_prob = day.precipitation_probability_max[0];
    std::string sunrise_time = extractTime(day.sunrise[0]);
    std::string sunset_time = extractTime(day.sunset[0]);

    // Направление ветра текстом
    std::string wind_dir_text = windDirectionText(wind_dir);

    // Вывод с фиксированной точностью
    std::cout << std::fixed << std::setprecision(1);
    if (detal) {
      std::cout << "Погода в "
                << toPrepositional(geoJson["name"].get<std::string>())
                << ": сейчас +" << temp << "°, ощущается +" << feels << "°, "
                << weather << ".\n";
      std::cout << "Влажность " << humidity << "%, ветер " << wind_speed
                << " км/ч (" << wind_dir_text << "), давление " << pressure
                << " гПа.\n";
      std::cout << "Сегодня: от " << t_min << "° до " << t_max << "°, "
                << precip_prob << "% дождя. Рассвет " << sunrise_time
                << ", закат " << sunset_time << ".\n";
    } else {
      std::cout << "Погода в " << geoJson["name"].get<std::string>() << " "
                << meteoJson.current.temperature_2m
                << meteoJson.current_units.temperature_2m << "\n\n";
    }

  } catch (const std::exception &e) {
    std::cout << "Ошибка получения погоды" << e.what() << "\n\n";
  }
  busy = false;
  return "";
}
