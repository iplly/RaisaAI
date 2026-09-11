#include "skills/model/LmTypes.h"
#include <string>

json weatherTool() {
  LLMTool tool;
  tool.function.name = "WeatherSkill";
  tool.function.description =
      "Получить текущую погоду в указанном городе. Используй ТОЛЬКО "
      "если пользователь спрашивает про погоду или температуру";
  tool.function.parameters.properties.insert(
      {{"city",
        {"string",
         "Название города на русском в именительном падеже: "
         "«Пермь», не «Перми», если город не указан оставить null"}}});
  return tool;
}

json vkmusicTool() {
  LLMTool tool;
  json jsonTool;
  tool.function.name = "VKMusicSkill";
  tool.function.description =
      "Включение музыки VK. Определи тип запроса по просьбе пользователя и "
      "заполни поля по правилам ниже.";
  tool.function.parameters.properties.insert(
      {"type",
       {"string", "search — конкретная песня, исполнитель или альбом; "
                  "playlist — плейлист, жанр или настроение; "
                  "mix — явная просьба включить микс; "
                  "my — \"моя музыка\", \"мои треки\", \"мои песни\""}});
  tool.function.parameters.properties.insert(
      {"track",
       {"string",
        "Название песни, исполнителя, альбома, плейлиста или жанра — "
        "без служебных слов («включи», «поставь», «вруби», «музыку», "
        "«плейлист», «песню» и т.п.). "
        "**Для type=mix или type=my оставь пустой строкой.** "
        "Для type=playlist переноси название плейлиста точно как в списке: "
        "«Для вас», «Плейлист недели», «Новинки», «Открытия», "
        "«Плейлист дня 1»…«Плейлист дня 5», «Плейлист в дорогу», "
        "«Грустно», "
        "«Радостно», «Активно», «Спокойно», «Новый год», «Любовь», "
        "«Электроника», «Хип-хоп», «Поп», «Фолк», «Утро»."}});
  tool.function.parameters.properties.insert(
      {"vibes",
       {"string",
        "Настроение микса (type=mix): happy — радостный, sad — грустный, "
        "active — активный, calm — спокойный, love — романтический. "
        "Заполняй только если пользователь явно назвал настроение, "
        "иначе оставь пустой строкой."}});
  tool.function.parameters.properties.insert(
      {"recognitions",
       {"string", "Тип микса (type=mix): known — знакомые/известные треки, "
                  "unknown — незнакомые, fresh — новинки. Заполняй только если "
                  "пользователь явно это указал, иначе пустая строка."}});
  tool.function.parameters.properties.insert(
      {"langs",
       {"string",
        "Язык микса (type=mix): ru — русскоязычная, international — "
        "иностранная, instrumental — инструментальная. Заполняй только "
        "если пользователь явно это указал, иначе пустая строка."}});
  tool.function.parameters.required = {"type"};
  jsonTool = tool;
  jsonTool["function"]["parameters"]["properties"]["type"]["enum"] = {
      "search", "playlist", "mix", "my"};
  jsonTool["function"]["parameters"]["properties"]["vibes"]["enum"] = {
      "happy", "sad", "active", "calm", "love"};
  jsonTool["function"]["parameters"]["properties"]["recognitions"]["enum"] = {
      "known", "unknown", "fresh"};
  jsonTool["function"]["parameters"]["properties"]["langs"]["enum"] = {
      "ru", "international", "instrumental"};
  return jsonTool;
}

json vkmusicAddTool() {
  LLMTool tool;
  tool.function.name = "vkmusicAddTool";
  tool.function.description =
      "Добавить трек в очередь воспроизведения. Из фразы выброси служебные "
      "слова: добавь, добавить, в очередь, песню, трек, эту, этот, давай, "
      "пожалуйста, мне. "
      "Примеры: «добавь в очередь Кино группа крови» → track=\"Кино группа "
      "крови\""
      "; «добавь песню Ария потерянный рай» → track=\"Ария потерянный рай\""
      ". В track — только название, без служебных слов.";
  tool.function.parameters.properties.insert(
      {"type",
       {"string", "Название трека/исполнителя без служебных слов («включи», "
                  "«поставь», «песню» и т.п.), как в примерах"}});
  tool.function.parameters.required = {"track"};
  return tool;
}

json timerTool() {
  LLMTool tool;
  json jsonTool;

  tool.function.name = "TimerSkill";
  tool.function.description =
      "Установить таймер или будильник. "
      "Примеры: «поставь таймер на 5 минут» → type=\"timer\", minutes=5; "
      "«разбуди в 7 утра» → type=\"alarm\", time=\"07:00\"; "
      "«будильник на полвосьмого» → type=\"alarm\", time=\"07:30\". "
      "Для type=timer заполняй только minutes; для type=alarm только time. "
      "Время всегда в 24-часовом формате ЧЧ:ММ.";
  tool.function.parameters.properties.insert(
      {"type",
       {"string", "timer — отсчёт от текущего момента; alarm — на "
                  "конкретное время"}});
  tool.function.parameters.properties.insert(
      {"minutes",
       {"integer", "Для type=timer: число минут от 1 до 1440. Для "
                   "alarm не заполняй."}});
  tool.function.parameters.properties.insert(
      {"time",
       {"string", "Для type=alarm: время ЧЧ:ММ, например \"07:30\". "
                  "Для timer не заполняй."}});
  tool.function.parameters.properties.insert(
      {"name",
       {"string", "Для названия таймера или будильника, при "
                  "отсутствии ставь \"общий\""}});
  tool.function.parameters.required = {"type"};
  jsonTool = tool;
  jsonTool["function"]["parameters"]["properties"]["type"].push_back(
      {"enum", {"timer", "alarm"}});
  return jsonTool;
}
