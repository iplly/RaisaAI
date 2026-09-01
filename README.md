# RaisaAI

Голосовой ассистент на C++20: локальное распознавание речи (Vosk + Whisper),
LLM через Ollama (диалог + роутер инструментов), скиллы: музыка VK, таймер,
погода, произвольный диалог.

## Содержание

- [Требования](#требования)
- [Сборка](#сборка)
- [Установка Vosk](#установка-vosk)
- [Конфигурация](#конфигурация)
  - [raisa.conf](#raisa-conf)
  - [vk.conf](#vk-conf)
- [setup.sh](#setupsh)
- [Запуск](#запуск)
- [Громкость](#громкость)
- [Структура проекта](#структура-проекта)

## Требования

**Сборка:**

- `g++` со стандартом C++20
- `cmake` (≥ 3.14)
- `pkg-config`
- FFmpeg dev: `libavdevice-dev libavformat-dev libavcodec-dev libavutil-dev libswresample-dev`
- libcurl: `libcurl4-openssl-dev`
- Vosk (см. [установка Vosk](#установка-vosk))
- Заголовочные библиотеки: `ctre.hpp` (compile-time regex) и `nlohmann/json.hpp`
  (`pacman -S compile-time-regular-expressions nlohmann-json`)
- необязательно (ускоряют сборку): `ccache`, `ninja`, `ld.lld`

**Запуск:**

- `mpv` (воспроизведение музыки), `socat` (управление mpv)
- PulseAudio или `pipewire-pulse` (захват микрофона, вход `pulse`)
- `ollama` с локальными моделями (см. [raisa.conf](#raisa-conf))
- Python-модуль `urllib3` (установится через `setup.sh` при необходимости)

## Сборка

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Исполняемый файл появится в `build/Raisa`. Можно собрать прямо в корне:

```bash
cmake -DCMAKE_BUILD_TYPE=Release .
make -j$(nproc)
```

## Установка Vosk

(Arch Linux)

```bash
pacman -S vosk-api
```

Устанавливает библиотеку `libvosk` и заголовок `vosk_api.h`.

## Конфигурация

### raisa.conf

| Ключ             | Назначение                                      |
| ---------------- | ----------------------------------------------- |
| `AUDIO_DEVICE`   | имя PulseAudio-входа микрофона                  |
| `AUDIO_RATE`     | частота дискретизации (используется и для Vosk) |
| `AUDIO_CHANNELS` | число каналов                                   |
| `VOSK_PATH`      | путь к модели Vosk                              |
| `WHISPER_URL`    | адрес whisper-server (`http://localhost:8000`)  |
| `OLLAMA_URL`     | адрес Ollama (`http://localhost:11434`)         |
| `LLM_MODEL`      | модель для обычного диалога                     |
| `ROUTER_MODEL`   | модель-роутер (выбор инструмента/скилла)        |

Пример:

```ini
AUDIO_DEVICE=Raisa
AUDIO_RATE=48000
AUDIO_CHANNELS=1
VOSK_PATH=./Models/vosk-model-small/
WHISPER_URL=http://127.0.0.1:8000/inference
OLLAMA_URL=http://localhost:11434
LLM_MODEL=gemma4:e4b
ROUTER_MODEL=gemma4:e4b
```

## setup.sh

Установщик проверяет зависимости и выполняет подготовку окружения:
загружает модель Vosk (`vosk-model-small-ru-0.22`, ~46 МБ с alphacephei.com),
клонирует репозитории `vk.py`/`vk_cookie_server.py`, при необходимости собирает
whisper-server, а затем запускает фоновые сервисы (whisper, vk-cookie-server).

```bash
./setup.sh              # полная проверка + запуск сервисов
./setup.sh --check-only # только проверка, ничего не запускать
./setup.sh --no-whisper # пропустить whisper
```

Расширение vk-ext в браузер ставится отдельно; из репозитория используется
только `vk_cookie_server.py`.

Модель whisper (например `ggml-podlodka-turbo-q8_0.bin`) поместите в `Models/`
или задайте путь через `export WHISPER_MODEL=/путь/к/модели.bin`.

Остановить сервисы:

```bash
kill $(cat /tmp/raisa/*.pid)
```

## Запуск

```bash
./setup.sh          # при первом запуске
./Raisa             # из корня проекта (после сборки)
```

## Структура проекта

```
.
├── CMakeLists.txt          # сборка (FFmpeg/libcurl — REQUIRED, Vosk — обязателен)
├── raisa.conf              # конфиг ассистента
├── vk.conf                 # секреты VK
├── setup.sh                # установщик/запуск сервисов
├── main.cpp                # точка входа: загрузка конфига, громкость, запуск
├── src/
│   ├── Config.cpp/.h       # парсер конфига (singleton)
│   ├── VoiceController.cpp # цикл прослушивания, распознавание, диалог
│   ├── vosk.cpp/.h         # распознавание команд (vosk_api.h)
│   ├── Ollama.cpp/.h       # запросы к LLM (OLLAMA_URL + /api/chat)
│   ├── control.cpp/.h      # setVolume(), сохранение громкости
│   ├── audio.cpp/.h        # захват Micro через av_find_input_format("pulse")
│   └── Skills/             # LlmSkill, TimerSkill, VkMusicSkill, WeatherSkill
├── services/               # systemd-юниты для whisper/vk-cookie-server
└── Models/                 # модели Vosk/whisper
```

`vk-ext/` — отдельный репозиторий Firefox-расширения и приёмника VK-кук.
См. `vk-ext/README.md`.
