#include "core/Config.h"
#include "core/Json.h"
#include "core/Process.h"
#include "net/Curl.h"
#include "skills/Skill.h"
#include "skills/model/LmTypes.h"
#include <exception>
#include <iostream>
#include <string>

std::string LLMSkill::name() const { return "LlmSkill"; }
std::string LLMSkill::description() const { return ""; }
bool LLMSkill::running() const { return busy.load(); }
void LLMSkill::stop() { stopFlag.store(true); }
std::string LLMSkill::execute(json j) {
  std::string message = j["message"];
  std::cout << "message: " << message << "\n\n";
  if (busy)
    return "Ждите";
  if (worker.joinable())
    worker.join();
  busy = true;
  stopFlag = false;
  worker = std::jthread(&LLMSkill::start, this, message);
  return "";
}

LLMSkill::LLMSkill() {
  LLMTool tool1;
  LLMTool tool2;
  LLMTool tool3;
  tool1.function.name = "get_aboba";
  tool1.function.description =
      "Пользователь просит получить абобу, абоба "
      "предоставляется в виде строки, после получения ответь пользователю "
      "что абоба получена, абоба получается исключительно один раз за сессию";
  tool1.function.parameters.properties.insert(
      {"aboba",
       {"string", "абоба, необходимо указывать если не сказано иное"}});
  tool2.function.name = "web_search";
  tool2.function.description =
      "web_search — поиск в DuckDuckGo. Правила:\n- Нужны свежие данные, "
      "факты, версии, новости \n— сначала ищи, потом "
      "отвечай.\n- Общие вопросы (синтаксис, "
      "определения, математика) — отвечай сразу без "
      "поиска.\n- Запрос делай коротким: 3–6 слов. "
      "Если результатов нет или они мусорные — "
      "переформулируй, не больше 3 попыток.\n- Не "
      "выдумывай URL, цифры, даты, названия. Не "
      "нашёл — так и скажи.\n- Отвечай "
      "на языке вопроса. Если по-русски нашлось "
      "мало — продублируй запрос на английском.\n- Не "
      "спамь поиском: 1–3 вызова на один "
      "вопрос.";
  tool2.function.parameters.properties.insert(
      {"query", {"string", "Запрос в интернет"}});
  tool3.function.name = "system_shell";
  tool3.function.description =
      "выполняет команду в bash и возвращает stdout, stderr и код возврата. "
      "Рабочая директория — текущий проект. Пользователь — обычный "
      "пользователь, не root.";
  tool3.function.parameters.properties.insert(
      {"command",
       {"string",
        "# ЧТО МОЖНО"
        "- Читать файлы: cat, less, head, tail, grep, rg, find, ls, tree, wc, "
        "file, stat."
        "- Искать: grep, rg, ag, find."
        "- Смотреть систему: pwd, whoami, id, uname, df, du, free, ps, top "
        "(только с -b), lscpu, lsblk."
        "- Работать с git: git status, git log, git diff, git show, git "
        "branch, git add, git commit, git stash."
        "- Собирать и запускать код проекта: make, cmake, g++, clang++, cargo, "
        "npm, python3, pip install --user."
        "- Запускать тесты: pytest, ctest, ./test*, make test."
        "- Управлять файлами внутри проекта: mkdir, touch, cp, mv, rm (только "
        "внутри проекта)."
        "- Скачивать: curl, wget (только на чтение, без пайпа в sh)."
        "# ЧТО НЕЛЬЗЯ (никогда, ни при каких условиях)"
        "- sudo, su, doas, pkexec — никакого повышения привилегий."
        "- rm -rf /, rm -rf /*, rm -rf ~, rm -rf $HOME, rm -rf .., любые rm с "
        "абсолютными путями вне проекта."
        "- dd, mkfs, fdisk, parted, mount, umount, swapoff — работа с дисками."
        "- chmod 777, chown, chgrp на системные пути."
        "- Изменение /etc, /boot, /usr, /bin, /sbin, /lib, /var, /opt, /root."
        "- Убийство процессов: kill -9 1, pkill -9, killall -9."
        "- Работа с ядром: modprobe, rmmod, insmod, sysctl -w."
        "- Сеть в режиме изменения: iptables, nft, ip link set, ifconfig down."
        "- Пайпы в shell: curl ... | sh, wget ... | bash, eval, exec, source "
        "<(curl ...)."
        "- Форк-бомбы: :(){ :|:& };:, любые бесконечные циклы с fork."
        "- Криптомайнеры, сканеры портов, брутфорс."
        "- Запись за пределами текущего проекта без явного разрешения "
        "пользователя."
        "- Любые команды, которые ты не понимаешь полностью."
        "# ПРАВИЛА РАБОТЫ"
        "1. Прежде чем выполнить команду — подумай, что она делает. Если "
        "сомневаешься — не выполняй, спроси пользователя."
        "2. Для разрушительных операций (rm, mv, overwrite) сначала покажи "
        "команду и спроси подтверждение."
        "3. Не используй sudo. Если команда требует root — скажи пользователю, "
        "пусть выполнит сам."
        "4. Не выполняй команды, которые ты не можешь объяснить построчно."
        "5. Не трогай файлы вне текущего проекта без явного разрешения."
        "6. Если команда вернула ошибку — прочитай её, не повторяй вслепую. Не "
        "больше 3 попыток на одну задачу."
        "7. Не устанавливай пакеты в систему. Используй pip install --user, "
        "venv, cargo, локальные сборки."
        "8. После изменений в проекте — покажи git diff, чтобы пользователь "
        "видел, что изменилось."
        "9. Не запускай долгие процессы без таймаута. Используй timeout 30 "
        "<cmd> для потенциально зависающих команд."
        "10. Если задача требует что-то вне разрешённого списка — остановись и "
        "объясни пользователю, что нужно сделать вручную."}});
  context.tools.push_back(std::move(tool1));
  context.tools.push_back(std::move(tool2));
}

void LLMSkill::start(std::string message) {
  try {
    bool done = false;
    Curl curlLlm(Config::instance().get("OLLAMA_URL") + "/api/chat");
    std::string headers = "Content-Type: application/json";
    LLMMessage userMessage = {"user", std::move(message)};
    context.messages.push_back(std::move(userMessage));

    curlLlm.addHeaders(headers);

    LLMMessage agentMessage = {"assistant", ""};
    int toolCalls = 0;
    while (!done) {
      bool isToolCall = false;
      json jsonData = context;
      std::cout << jsonData.dump(2) << "\n\n";
      LLMMessage tool;

      std::string sb;
      curlLlm.post(jsonData, [&](const char *chunk, size_t len) -> size_t {
        stream.full.clear();
        sb.append(chunk, len);
        size_t pos;

        while ((pos = sb.find('\n')) != std::string::npos) {
          std::string line = sb.substr(0, pos);
          sb.erase(0, pos + 1);

          if (line.empty())
            continue;
          json j;

          try {
            j = json::parse(line);
          } catch (const std::exception &e) {
            continue;
          }

          if (!j.contains("message"))
            continue;

          if (j["message"].contains("tool_calls")) {
            ++toolCalls;
            isToolCall = true;

            std::cout << j["message"]["tool_calls"] << "\n\n";
            agentMessage.tool_calls = j["message"]["tool_calls"];
            tool = ToolChoser(j["message"]["tool_calls"])[0];

            json toolJson = tool;
            std::cout << toolJson.dump(2) << "\n\n";
          }
          std::string token = j["message"].value("content", "");
          std::cout << token << std::flush;
          stream.full.append(token);
          agentMessage.content.append(token);
          if (stopFlag)
            return 0;
        }
        return len;
      }); // curlPost end

      if ((!isToolCall || toolCalls > 3)) {
        done = true;
      }
      std::cout << "\ndone: " << done << " toolCalls: " << toolCalls << "\n\n";

      context.messages.push_back(std::move(agentMessage));
      context.messages.push_back(tool);
    }

    // std::cout << stream.full << "\n\n";

    busy = false;
  } catch (std::exception &e) {
    std::cout << "Ошибка LLM: " << e.what() << "\n\n";
    busy = false;
  }
}
std::vector<LLMMessage> LLMSkill::ToolChoser(const json &tools) {
  std::vector<LLMMessage> msgTools;
  for (const auto &tool : tools) {
    LLMMessage msg;
    msg.role = "tool";
    msg.tool_call_id = tool["id"].get<std::string>();
    std::string name = tool["function"]["name"].get<std::string>();
    json arguments = tool["function"]["arguments"];
    try {
      if (name == "get_aboba") {
        msg.content =
            "Полученна абоба " + arguments["aboba"].get<std::string>();
      }
      if (name == "web_search") {
        json res = json::parse(exec("python3 src/ddg/ddg.py search " +
                                    arguments["query"].dump()))["results"];
        msg.content = res.dump();
      }
      if (name == "system_shell") {
        msg.content = exec(arguments["command"].get<std::string>());
      }
    } catch (const std::exception &e) {
      std::cout << "Ошибка вызовы инструмента агентом: " << e.what() << "\n\n";
      msg.content = "Ошибка инструмента";
    }
    msgTools.push_back(msg);
  }
  return msgTools;
}
