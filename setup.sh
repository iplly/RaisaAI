#!/usr/bin/env bash
# =============================================================================
# setup.sh — RaisaAI: проверка зависимостей, скачивание моделей, запуск сервисов
#
# Usage:
#   ./setup.sh              # полная проверка + запуск сервисов
#   ./setup.sh --check-only # только проверить, не запускать
#   ./setup.sh --no-whisper # пропустить whisper
# =============================================================================

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODELS_DIR="$ROOT/Models"
VOSK_MODEL_DIR="$MODELS_DIR/vosk-model-small"
# Зеркала vosk-model-small-ru-0.22.zip: официальное (alphacephei.com) качает
# ~200 байт/c — почти всегда виснет. Первым идёт быстрое зеркало на HuggingFace
# (rhasspy/vosk-models), официальное остаётся запасным.
VOSK_URLS=(
  "https://huggingface.co/rhasspy/vosk-models/resolve/main/ru/vosk-model-small-ru-0.22.zip"
  "https://alphacephei.com/vosk/models/vosk-model-small-ru-0.22.zip"
)
# Временный файл/каталог для распаковки кладём рядом с модели (внутри проекта),
# чтобы скрипт был переносим между пользователями/машинами.
VOSK_ZIP="$MODELS_DIR/vosk-model-small-ru-0.22.zip"
VOSK_TMP="$MODELS_DIR/.tmp-vosk"
WHISPER_PORT=8000
# TTS (qwen-talker). Модели лежат в $MODELS_DIR; имена переопределяемые.
TTS_PORT=8001
TTS_MODEL_BASE="${TTS_MODEL_BASE:-qwen-talker-1.7b-customvoice-Q8_0.gguf}"
TTS_MODEL_CODEC="${TTS_MODEL_CODEC:-qwen-tokenizer-12hz-Q8_0.gguf}"
TTS_ALIAS="qwen3-tts-base"
# PID и логи — в каталог времени выполнения (переносимо для любого пользователя)
RUN_DIR="${XDG_RUNTIME_DIR:-/tmp}/raisa"
PIDFILE_DIR="$RUN_DIR"
LOG_DIR="$RUN_DIR"

# --- Репозитории внешних компонентов (впишите ваши ссылки) ---
# vk.py: Python-мост к VK Music API (внутри каталога src/vkmusic, в этом репозитории)
VK_PY_REPO="${VK_PY_REPO:-}"
# vk-ext: расширение Firefox + приёмник VK-кук (браузер ставится отдельно)
VK_EXT_REPO="https://github.com/iplly/vk-ext"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

check_only=false
skip_whisper=false

for arg in "$@"; do
  case "$arg" in
  --check-only) check_only=true ;;
  --no-whisper) skip_whisper=true ;;
  esac
done

ok() { echo -e "  ${GREEN}✓${NC} $1"; }
warn() { echo -e "  ${YELLOW}!${NC} $1"; }
fail() { echo -e "  ${RED}✗${NC} $1"; }

port_in_use() {
  python3 -c "
import socket, sys
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    s.bind(('127.0.0.1', int(sys.argv[1])))
    s.close()
    sys.exit(1)
except OSError:
    sys.exit(0)
" "$1"
}

echo "=== RaisaAI setup ==="
echo ""

# --- 1. Базовые зависимости ---
echo "1. Проверка базовых зависимостей..."
for cmd in python3 curl unzip; do
  if command -v "$cmd" >/dev/null 2>&1; then
    ok "$cmd: $(command -v "$cmd")"
  else
    fail "$cmd: не найден. Установите вручную."
  fi
done
echo ""

# --- 2. Python-модули ---
echo "2. Проверка Python-модулей..."
if python3 -c "import urllib3" 2>/dev/null; then
  ok "urllib3"
else
  warn "urllib3 не найден. Устанавливаю..."
  pip3 install --quiet urllib3 2>/dev/null && ok "urllib3 установлен" || fail "Не удалось установить urllib3"
fi
echo ""

# --- 3. Ollama ---
echo "3. Проверка Ollama..."
if command -v ollama >/dev/null 2>&1; then
  ok "ollama: $(command -v ollama)"
  if curl -sf --max-time 5 http://localhost:11434/api/tags >/dev/null 2>&1; then
    ok "Ollama daemon работает (localhost:11434)"
    model_count=$(ollama list 2>/dev/null | tail -n +2 | wc -l)
    if [ "$model_count" -gt 0 ]; then
      ok "Модели: $(ollama list 2>/dev/null | tail -n +2 | awk '{print $1}' | tr '\n' ' ')"
    else
      warn "Нет загруженных моделей. Загрузите нужные модели через ollama pull"
    fi
  else
    warn "Ollama daemon не отвечает. Запустите: ollama serve"
  fi
else
  fail "ollama не найден. Установите: https://ollama.com/download"
fi
echo ""

# --- 4. Vosk модель ---
echo "4. Проверка Vosk модели..."
if [ -d "$VOSK_MODEL_DIR/am" ]; then
  ok "vosk-model-small: $(du -sh "$VOSK_MODEL_DIR" | cut -f1)"
else
  warn "vosk-model-small не найдена в $VOSK_MODEL_DIR"
  mkdir -p "$MODELS_DIR"
  VOSK_EXPECTED=46236750 # известный размер (байт)
  VOSK_OK=false
  for src in "${VOSK_URLS[@]}"; do
    echo "  Скачиваю vosk-model-small-ru-0.22 (46 МБ) с ${src%%://*}..."
    rm -f "$VOSK_ZIP"
    # Однократная попытка на зеркало, лимит 300 c. Официальное зеркало
    # медленное — при неудаче сразу переходим к следующему.
    if timeout --foreground 300 curl -fL \
      --connect-timeout 20 \
      --max-time 280 \
      -o "$VOSK_ZIP" "$src" 2>/dev/null; then
      VOSK_ACTUAL=$(stat -c%s "$VOSK_ZIP" 2>/dev/null || echo 0)
      if [ "$VOSK_ACTUAL" -ge "$VOSK_EXPECTED" ]; then
        VOSK_OK=true
        break
      fi
      rm -f "$VOSK_ZIP"
    fi
  done
  if [ "$VOSK_OK" = true ]; then
    unzip -qo "$VOSK_ZIP" -d "$VOSK_TMP"
    if [ -d "$VOSK_TMP/vosk-model-small-ru-0.22" ]; then
      rm -rf "$VOSK_MODEL_DIR"
      mv "$VOSK_TMP/vosk-model-small-ru-0.22" "$VOSK_MODEL_DIR"
      rm -rf "$VOSK_TMP"
      rm -f "$VOSK_ZIP"
      ok "vosk-model-small установлена"
    else
      rm -rf "$VOSK_TMP"
      fail "Не удалось распаковать модель (ожидалась папка vosk-model-small-ru-0.22)"
    fi
  else
    fail "Не удалось скачать модель ни с одного зеркала:"
    for src in "${VOSK_URLS[@]}"; do
      echo "    $src"
    done
    echo "  Либо скачайте вручную и распакуйте в $VOSK_MODEL_DIR"
  fi
fi
echo ""

# --- 4.5. Python-мост vk.py из git ---
echo "5. Python-мост vk.py..."
ensure_repo() {
  local url="$1" dest="$2"
  if [ -d "$dest/.git" ]; then
    (cd "$dest" && git pull --quiet --ff-only) && ok "$(basename "$dest"): обновлён" || warn "$(basename "$dest"): не удалось обновить"
  elif [ -d "$dest" ]; then
    warn "$(basename "$dest"): есть локально (без .git), не трогаю"
  else
    echo "  Клонирую $(basename "$dest") из $url ..."
    if git clone --depth 1 "$url" "$dest" >/dev/null 2>&1; then
      ok "$(basename "$dest"): склонирован"
    else
      fail "Не удалось склонировать $(basename "$dest") из $url"
    fi
  fi
}

if [ -n "$VK_PY_REPO" ]; then
  ensure_repo "$VK_PY_REPO" "$ROOT/src/vkmusic"
else
  if [ -f "$ROOT/src/vkmusic/vk.py" ]; then
    ok "vk.py: локально ($(basename "$ROOT/src/vkmusic/vk.py"))"
  else
    warn "vk.py не найден. Укажите VK_PY_REPO в setup.sh"
  fi
fi

if [ -n "$VK_EXT_REPO" ]; then
  ensure_repo "$VK_EXT_REPO" "$ROOT/vk-ext"
else
  warn "VK_EXT_REPO не задан — vk-ext пропущен"
fi

# --- 6. Whisper ---
if [ "$skip_whisper" = false ]; then
  echo "6. Проверка Whisper..."
  WHISPER_BIN=""
  for candidate in \
    "$ROOT/whisper-server" \
    "$ROOT/whisper.cpp/build/bin/whisper-server" \
    "$ROOT/build/bin/whisper-server"; do
    if [ -x "$candidate" ]; then
      WHISPER_BIN="$candidate"
      break
    fi
  done
  if [ -z "$WHISPER_BIN" ] && command -v whisper-server >/dev/null 2>&1; then
    WHISPER_BIN="$(command -v whisper-server)"
  fi

  if [ -n "$WHISPER_BIN" ]; then
    ok "whisper-server: $WHISPER_BIN"
  else
    fail "whisper-server не найден"
    read -rp "  Установить whisper.cpp из исходников? [y/N] " install_whisper
    if [ "$install_whisper" = "y" ] || [ "$install_whisper" = "Y" ]; then
      echo "  Клонирую whisper.cpp..."
      WHISPER_SRC="$ROOT/whisper.cpp"
      if [ -d "$WHISPER_SRC" ]; then
        ok "whisper.cpp уже есть в $WHISPER_SRC"
      else
        mkdir -p "$(dirname "$WHISPER_SRC")"
        git clone --depth 1 https://github.com/ggerganov/whisper.cpp "$WHISPER_SRC"
      fi
      echo "  Собираю whisper.cpp..."
      cd "$WHISPER_SRC"
      # Vulkan: если есть заголовки/либа — включаем ускорение GGML_VULKAN=ON.
      # Иначе явно GGML_VULKAN=OFF (иначе кэш прошлой сборки может удержать ON).
      if pkg-config --exists vulkan 2>/dev/null; then
        ok "Vulkan: найден (pkg-config), включаю GGML_VULKAN=ON"
        VULKAN_CMAKE_ARGS=(-DGGML_VULKAN=ON)
      else
        warn "Vulkan не найден (нужен vulkan-header + vulkan-loader). Собираю без GPU-ускорения (CPU)."
        warn "  Debian/Ubuntu: sudo apt install libvulkan-dev"
        warn "  Arch:           sudo pacman -S vulkan-headers vulkan-icd-loader"
        VULKAN_CMAKE_ARGS=(-DGGML_VULKAN=OFF)
      fi
      CMAKE_LOG="$WHISPER_SRC/.skip-whisper-cmake.log"
      if ! cmake -B build -DCMAKE_BUILD_TYPE=Release "${VULKAN_CMAKE_ARGS[@]}" >"$CMAKE_LOG" 2>&1; then
        fail "cmake упал (хвост лога: $CMAKE_LOG)"
        tail -20 "$CMAKE_LOG" | sed 's/^/    /'
        cd "$ROOT"
        exit 1
      fi
      if ! cmake --build build -j"$(nproc)" --target whisper-server >>"$CMAKE_LOG" 2>&1; then
        fail "Сборка whisper-server упала (хвост лога: $CMAKE_LOG)"
        tail -20 "$CMAKE_LOG" | sed 's/^/    /'
        cd "$ROOT"
        exit 1
      fi
      cd "$ROOT"
      WHISPER_BIN="$WHISPER_SRC/build/bin/whisper-server"
      if [ -x "$WHISPER_BIN" ]; then
        ok "whisper-server собран: $WHISPER_BIN"
      else
        fail "Не удалось собрать whisper-server"
      fi
    else
      echo "  Пропущено. Установите вручную:"
      echo "    git clone https://github.com/ggerganov/whisper.cpp $ROOT/whisper.cpp"
      echo "    cd $ROOT/whisper.cpp && cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build"
      echo "  Модель для скачивания (положите в $MODELS_DIR):"
      echo "    Hugging Face: ggml-podlodka-turbo-q8_0.bin"
      echo "    export WHISPER_MODEL=$MODELS_DIR/ggml-podlodka-turbo-q8_0.bin"
    fi
  fi
  echo ""
else
  echo "6. Проверка Whisper... пропущена (--no-whisper)"
  echo ""
fi

# --- 7. Запуск сервисов ---
if [ "$check_only" = true ]; then
  echo "Проверка портов (check-only)..."
else
  echo "7. Запуск сервисов..."
  mkdir -p "$PIDFILE_DIR" "$LOG_DIR"
fi

# --- 7a. Whisper server ---
echo "   whisper-server (:$WHISPER_PORT)..."
if port_in_use "$WHISPER_PORT"; then
  ok "Порт $WHISPER_PORT уже занят (whisper-server работает)"
else
  if [ "$check_only" = true ]; then
    warn "Порт $WHISPER_PORT свободен (whisper-server не запущен)"
  elif [ -n "${WHISPER_BIN:-}" ] && [ -x "${WHISPER_BIN:-}" ]; then
    WHISPER_MODEL_PATH="${WHISPER_MODEL:-}"
    if [ -z "$WHISPER_MODEL_PATH" ]; then
      # по умолчанию ищем модель в $MODELS_DIR (переносимо)
      if [ -f "$MODELS_DIR/ggml-podlodka-turbo-q8_0.bin" ]; then
        WHISPER_MODEL_PATH="$MODELS_DIR/ggml-podlodka-turbo-q8_0.bin"
      else
        warn "WHISPER_MODEL не задан и модель не найдена в $MODELS_DIR."
        echo "    Положите ggml-podlodka-turbo-q8_0.bin в $MODELS_DIR"
        echo "    или укажите: export WHISPER_MODEL=/путь/к/модели.bin"
        echo "    затем: ./setup.sh"
      fi
    fi
    if [ -n "$WHISPER_MODEL_PATH" ]; then
      nohup "$WHISPER_BIN" \
        -m "$WHISPER_MODEL_PATH" \
        --host 0.0.0.0 \
        --port "$WHISPER_PORT" \
        -l ru \
        >"$LOG_DIR/raisa-whisper.log" 2>&1 &
      echo $! >"$PIDFILE_DIR/whisper-server.pid"
      # Модель грузится не мгновенно — ждём порт до 30 c.
      WHISPER_UP=false
      for _ in $(seq 1 30); do
        if port_in_use "$WHISPER_PORT"; then
          WHISPER_UP=true
          break
        fi
        sleep 1
      done
      if [ "$WHISPER_UP" = true ]; then
        ok "whisper-server запущен (PID $(cat "$PIDFILE_DIR/whisper-server.pid"))"
      else
        fail "whisper-server не запустился (см. $LOG_DIR/raisa-whisper.log)"
        tail -20 "$LOG_DIR/raisa-whisper.log" | sed 's/^/    /'
      fi
    fi
  else
    warn "whisper-server не найден, пропуск"
  fi
fi

# --- 7b. TTS server (qwen-talker) ---
echo "   tts-server (:$TTS_PORT)..."
TTS_BIN=""
for candidate in \
  "$ROOT/qwentts.cpp/build/tts-server" \
  "$ROOT/qwentts.cpp/build/bin/tts-server"; do
  if [ -x "$candidate" ]; then
    TTS_BIN="$candidate"
    break
  fi
done
if [ -z "$TTS_BIN" ] && command -v tts-server >/dev/null 2>&1; then
  TTS_BIN="$(command -v tts-server)"
fi
if port_in_use "$TTS_PORT"; then
  ok "Порт $TTS_PORT уже занят (tts-server работает)"
else
  if [ "$check_only" = true ]; then
    warn "Порт $TTS_PORT свободен (tts-server не запущен)"
  elif [ -n "$TTS_BIN" ]; then
    TTS_MODEL_PATH="$MODELS_DIR/$TTS_MODEL_BASE"
    TTS_CODEC_PATH="$MODELS_DIR/$TTS_MODEL_CODEC"
    if [ -f "$TTS_MODEL_PATH" ] && [ -f "$TTS_CODEC_PATH" ]; then
      nohup "$TTS_BIN" \
        --model "$TTS_MODEL_PATH" \
        --codec "$TTS_CODEC_PATH" \
        --alias "$TTS_ALIAS" \
        --port "$TTS_PORT" \
        >"$LOG_DIR/raisa-tts.log" 2>&1 &
      echo $! >"$PIDFILE_DIR/tts-server.pid"
      # Модель (2+ ГБ) грузится дольше секунды — ждём порт до 30 c.
      TTS_UP=false
      for _ in $(seq 1 30); do
        if port_in_use "$TTS_PORT"; then
          TTS_UP=true
          break
        fi
        sleep 1
      done
      if [ "$TTS_UP" = true ]; then
        ok "tts-server запущен (PID $(cat "$PIDFILE_DIR/tts-server.pid"))"
      else
        fail "tts-server не запустился (см. $LOG_DIR/raisa-tts.log)"
        tail -20 "$LOG_DIR/raisa-tts.log" | sed 's/^/    /'
      fi
    else
      warn "Модели TTS не найдены в $MODELS_DIR:"
      echo "    $TTS_MODEL_BASE"
      echo "    $TTS_MODEL_CODEC"
      echo "    Положите их в $MODELS_DIR "
    fi
  else
    warn "tts-server не найден, пропуск "
  fi
fi

# --- 7c. vk.conf ---
echo "8. Проверка vk.conf..."
if [ -f "$ROOT/vk.conf" ]; then
  ok "vk.conf существует"
  # Секретный файл: доступ только владельцу (600).
  chmod 600 "$ROOT/vk.conf"
  if grep -q "^VK_ACCESS_TOKEN=" "$ROOT/vk.conf" && grep -q "^VK_COOKIE=" "$ROOT/vk.conf"; then
    ok "VK_ACCESS_TOKEN и VK_COOKIE присутствуют"
  else
    warn "vk.conf неполный. Нужны VK_ACCESS_TOKEN= и VK_COOKIE="
  fi
else
  warn "vk.conf не найден — создаю шаблон..."
  cat >"$ROOT/vk.conf" <<'EOF'
VK_ACCESS_TOKEN=
VK_COOKIE=
EOF
  if [ -f "$ROOT/vk.conf" ]; then
    chmod 600 "$ROOT/vk.conf"
    ok "vk.conf создан ($ROOT/vk.conf)"
    warn "Заполните VK_ACCESS_TOKEN и VK_COOKIE вручную (куки из браузера: remixsid, p)."
  else
    fail "Не удалось создать vk.conf"
  fi
fi
echo ""

# --- 9. Итого ---
echo "=== Готово ==="
echo ""
echo "Для запуска RaisaAI:"
echo "  cd $ROOT && ./Raisa"
echo ""
echo "Полезные команды:"
echo "  ollama list                  — список моделей Ollama"
echo "  kill \$(cat $PIDFILE_DIR/*.pid)  — остановить сервисы"
