#include "core/Text.h"
#include "net/Curl.h"
#include "skills/Skill.h"
#include "skills/SkillContext.h"
#include "skills/model/WeatherTypes.h"
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

static std::string windDirectionText(int degrees) {
  const std::vector<std::string> dirs = {
      "Северный", "Северо восточный", "Восточный", "Юго восточный",
      "Южный",    "Юго западный",     "Западный",  "Северо западный"};
  int idx = static_cast<int>((degrees + 22.5) / 45.0) % 8;
  return dirs[idx];
}
static std::string fractionName(std::size_t digits) {
  switch (digits) {
  case 1:
    return " десят";
  case 2:
    return " сот";
  case 3:
    return " тысячн";
  case 4:
    return " десятитысячн";
  default:
    return "";
  }
}
static std::string morfSet(std::string digits) {
  if (digits.back() == '1' && digits != "11")
    return "ая ";
  else
    return "ых ";
}

static std::string doubleToSplitString(double d) {

  std::ostringstream os;
  os << std::fixed << std::setprecision(2) << d;
  std::string str_d = os.str();
  auto pos = str_d.find('.');
  if (pos == std::string::npos)
    return str_d;
  std::string whole, frac;
  whole = str_d.substr(0, pos);
  frac = str_d.substr(pos + 1);

  return numbertowords(std::stoi(whole)) + " цел" + morfSet(whole) +
         numbertowords(std::stoi(frac)) + fractionName(frac.size()) +
         morfSet(frac);
}

static std::string extractTime(long long unix_sec, int tz_offset_sec) {
  std::time_t t = unix_sec + tz_offset_sec;
  std::tm tm{};
  gmtime_r(&t, &tm);
  char bufH[8];
  char bufM[8];
  std::strftime(bufH, sizeof(bufH), "%H", &tm);
  std::strftime(bufM, sizeof(bufM), "%M", &tm);
  return numbertowords(std::stoi(std::string(bufH))) + " часов " +
         numbertowords(std::stoi(std::string(bufM))) + " минут";
}
static double dewPoint(double T, double humidity) {
  constexpr double a = 17.27;
  constexpr double b = 237.7;

  // На всякий случай защищаемся от некорректных входных данных
  if (humidity <= 0.0)
    return -273.15; // сухой воздух → бесконечно низкая точка росы
  if (humidity > 100.0)
    humidity = 100.0;

  double alpha = (a * T) / (b + T) + std::log(humidity / 100.0);
  return (b * alpha) / (a - alpha);
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
  std::string message;

  try {
    Geo geo = geoCoding(city);

    WeatherOWMResponse meteo = owmProvider(geo.lat, geo.lon);
    double temp = meteo.main.temp;
    double feels = meteo.main.feels_like;
    double temp_min = meteo.main.temp_min;
    double temp_max = meteo.main.temp_max;
    int humidity = meteo.main.humidity;
    int pressure = meteo.main.pressure;
    double wind_speed = meteo.wind.speed;
    std::string weather =
        meteo.weather.empty() ? "" : meteo.weather.front().description;
    std::string wind_dir = windDirectionText(meteo.wind.deg);
    std::string sunrise = extractTime(meteo.sys.sunrise, meteo.timezone);
    std::string sunset = extractTime(meteo.sys.sunset, meteo.timezone);
    city = meteo.name;

    message =
        "Погода в " + toPrepositional(city) + ": сейчас " +
        doubleToSplitString(temp) + " градусов, ощущается как " +
        doubleToSplitString(feels) + " градусов, " + weather + ".\n" +
        "Влажность " + numbertowords(humidity) + " процентов, точка росы " +
        doubleToSplitString(dewPoint(meteo.main.temp, meteo.main.humidity)) +
        ", ветер " + doubleToSplitString(wind_speed) + " метров в секунду (" +
        wind_dir + "), давление " + numbertowords(pressure) +
        " гектопаскаль.\n" + "Сегодня: от " + doubleToSplitString(temp_min) +
        " градусов до " + doubleToSplitString(temp_max) +
        " градусов. Рассвет в " + sunrise + ", закат в " + sunset + ".\n";

    g_skills.tts->speak(message);

  } catch (const std::exception &e) {
    std::cout << "Ошибка получения погоды" << e.what() << "\n\n";
  }
  busy = false;
  return message;
}

WeatherSkill::Geo WeatherSkill::geoCoding(const std::string &city) {
  Curl curlIP("https://free.freeipapi.com/api/v1/json/");
  Curl curlGeo("https://geocoding-api.open-meteo.com/v1/");
  std::string result;
  std::string cityResult = city;
  if (city == "null") {
    curlIP.get("", [&](const char *data, size_t len) -> size_t {
      result.append(data, len);
      return len;
    });
    json curlJson = json::parse(result);
    cityResult = curlJson["cityName"];
    result.clear();
  }

  curlGeo.get("search?name=" + urlEncode(cityResult) + "&count=1&language=ru",
              [&](const char *data, size_t len) -> size_t {
                result.append(data, len);
                return len;
              });
  json geoJson = json::parse(result)["results"][0];
  return {.lat = geoJson["latitude"].get<double>(),
          .lon = geoJson["longitude"].get<double>()};
}

WeatherOWMResponse WeatherSkill::owmProvider(double lat, double lon) {
#include "../../owm.h" //WARN: ВРЕМЕННО
  Curl curlMeteo("https://api.openweathermap.org/data/2.5/weather");
  std::string result;
  curlMeteo.get("?lat=" + std::to_string(lat) + "&lon=" + std::to_string(lon) +
                    "&appid=" + OPENWEATHERMAPAPI + "&units=metric&lang=ru",
                [&](const char *data, size_t len) -> size_t {
                  result.append(data, len);
                  return len;
                });
  WeatherOWMResponse owmResponse = json::parse(result);
  return owmResponse;
}
