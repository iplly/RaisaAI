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
VOSK_URL="https://alphacephei.com/vosk/models/vosk-model-small-ru-0.22.zip"
# Временный файл/каталог для распаковки кладём рядом с модели (внутри проекта),
# чтобы скрипт был переносим между пользователями/машинами.
VOSK_ZIP="$MODELS_DIR/vosk-model-small-ru-0.22.zip"
VOSK_TMP="$MODELS_DIR/.tmp-vosk"
WHISPER_PORT=8000
VK_COOKIE_PORT=8002
# PID и логи — в каталог времени выполнения (переносимо для любого пользователя)
RUN_DIR="${XDG_RUNTIME_DIR:-/tmp}/raisa"
PIDFILE_DIR="$RUN_DIR"
LOG_DIR="$RUN_DIR"

# --- Репозитории внешних компонентов (впишите ваши ссылки) ---
# vk.py: Python-мост к VK Music API
VK_PY_REPO="${VK_PY_REPO:-}"
# vk-ext: репозиторий (содержит vk_cookie_server.py; расширение ставится в браузер отдельно)
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
for cmd in python3 curl wget unzip tar; do
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
  echo "  Скачиваю vosk-model-small-ru-0.22 (46 МБ)..."
  echo "  (при медленном соединении загрузка прервётся сама — докачается при повторном запуске)"
  VOSK_EXPECTED=46236750 # известный Content-Length (байт)
  # Детерминированный лимит: одна попытка, жёсткий таймаут 90 сек,
  # при обрыве -C - докачает при следующем запуске. Не зависает.
  if timeout --foreground 90 curl -fL -C - \
    --connect-timeout 20 \
    -o "$VOSK_ZIP" "$VOSK_URL" 2>/dev/null; then
    VOSK_ACTUAL=$(stat -c%s "$VOSK_ZIP" 2>/dev/null || echo 0)
    if [ "$VOSK_ACTUAL" -lt "$VOSK_EXPECTED" ]; then
      fail "Модель скачалась не полностью ($VOSK_ACTUAL из $VOSK_EXPECTED байт)"
      rm -f "$VOSK_ZIP"
      echo "  Сеть до alphacephei.com медленная. Повторите ./setup.sh позже (загрузка докачается)."
      echo "  Либо скачайте вручную и распакуйте в $VOSK_MODEL_DIR:"
      echo "    wget $VOSK_URL"
      echo "    unzip vosk-model-small-ru-0.22.zip"
      echo "    mv vosk-model-small-ru-0.22 $VOSK_MODEL_DIR"
    else
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
    fi
  else
    fail "Не удалось скачать модель с $VOSK_URL"
    echo "  Сеть до alphacephei.com медленная или недоступна. Повторите позже."
    echo "  Либо скачайте вручную и распакуйте в $VOSK_MODEL_DIR:"
    echo "    wget $VOSK_URL"
    echo "    unzip vosk-model-small-ru-0.22.zip"
    echo "    mv vosk-model-small-ru-0.22 $VOSK_MODEL_DIR"
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

echo "   vk-cookie-server (из vk-ext репозитория)..."
if [ -n "$VK_EXT_REPO" ]; then
  ensure_repo "$VK_EXT_REPO" "$ROOT/vk-ext"
  if [ -f "$ROOT/vk-ext/vk_cookie_server.py" ]; then
    ok "vk_cookie_server.py: доступен ($ROOT/vk-ext/vk_cookie_server.py)"
  else
    fail "vk_cookie_server.py не найден в $ROOT/vk-ext"
  fi
else
  if [ -f "$ROOT/vk-ext/vk_cookie_server.py" ]; then
    ok "vk_cookie_server.py: локально"
  else
    warn "vk_cookie_server.py не найден. Укажите VK_EXT_REPO в setup.sh"
  fi
fi
echo ""

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
      cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
      cmake --build build -j"$(nproc)" --target whisper-server >/dev/null 2>&1
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
      sleep 1
      if port_in_use "$WHISPER_PORT"; then
        ok "whisper-server запущен (PID $(cat "$PIDFILE_DIR/whisper-server.pid"))"
      else
        fail "whisper-server не запустился (см. $LOG_DIR/raisa-whisper.log)"
      fi
    fi
  else
    warn "whisper-server не найден, пропуск"
  fi
fi

# --- 7b. VK Cookie Server ---
echo "   vk-cookie-server (:$VK_COOKIE_PORT)..."
if port_in_use "$VK_COOKIE_PORT"; then
  ok "Порт $VK_COOKIE_PORT уже занят (vk-cookie-server работает)"
else
  if [ "$check_only" = true ]; then
    warn "Порт $VK_COOKIE_PORT свободен (vk-cookie-server не запущен)"
  else
    if [ -f "$ROOT/vk-ext/vk_cookie_server.py" ]; then
      nohup python3 "$ROOT/vk-ext/vk_cookie_server.py" \
        --config "$ROOT/vk.conf" \
        >"$LOG_DIR/raisa-vk-cookie.log" 2>&1 &
      echo $! >"$PIDFILE_DIR/vk-cookie-server.pid"
      sleep 1
      if port_in_use "$VK_COOKIE_PORT"; then
        ok "vk-cookie-server запущен (PID $(cat "$PIDFILE_DIR/vk-cookie-server.pid"))"
      else
        fail "vk-cookie-server не запустился (см. $LOG_DIR/raisa-vk-cookie.log)"
      fi
    else
      fail "vk_cookie_server.py не найден"
    fi
  fi
fi
echo ""

# --- 8. vk.conf ---
echo "8. Проверка vk.conf..."
if [ -f "$ROOT/vk.conf" ]; then
  ok "vk.conf существует"
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
    ok "vk.conf создан ($ROOT/vk.conf)"
    warn "Заполните в нём VK_ACCESS_TOKEN и VK_COOKIE (см. vk-ext)."
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
