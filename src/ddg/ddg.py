#!/usr/bin/env python3
"""Мост Raisa -> DuckDuckGo (поиск по интернету).

CLI, всегда печатает один JSON в stdout и завершается с кодом 0:

    python3 ddg.py search <запрос> [количество]  -> {"ok": true, "results": [{"title", "href", "body"}, ...]}
    python3 ddg.py news <запрос> [количество]    -> {"ok": true, "results": [{"title", "url", "date", "body"}, ...]}

Любая ошибка -> {"ok": false, "error": "..."} (процесс НЕ падает).
"""

import json
import os
import sys
import time

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "vendor")
)

try:
    from ddgs import DDGS
except Exception as e:
    print(
        json.dumps({"ok": False, "error": f"ddgs не доступен: {e}"}, ensure_ascii=False)
    )
    sys.exit(0)


def _text(ddgs, query, count=5):
    raw = list(ddgs.text(query, max_results=count))
    return [
        {
            "title": r.get("title") or "",
            "href": r.get("href") or "",
            "body": r.get("body") or "",
        }
        for r in raw
    ]


def _news(ddgs, query, count=5):
    raw = list(ddgs.news(query, max_results=count))
    return [
        {
            "title": r.get("title") or "",
            "url": r.get("url") or r.get("href") or "",
            "date": r.get("date") or "",
            "body": r.get("body") or "",
        }
        for r in raw
    ]


def run(kind, query, count=5):
    last_err = None
    for attempt in range(2):
        try:
            with DDGS() as ddgs:
                if kind == "news":
                    results = _news(ddgs, query, count)
                else:
                    results = _text(ddgs, query, count)
            if not results:
                return {"ok": False, "error": "ничего не найдено"}
            return {"ok": True, "results": results}
        except Exception as e:
            last_err = e
            time.sleep(1 + attempt)
    return {"ok": False, "error": str(last_err)}


def main():
    args = sys.argv[1:]
    try:
        if len(args) < 2:
            payload = {
                "ok": False,
                "error": f"usage: {sys.argv[0]} search|news <запрос> [количество]",
            }
        elif args[0] not in ("search", "news"):
            payload = {"ok": False, "error": f"неизвестная команда: {args[0]}"}
        else:
            count = int(args[len(args) - 1]) if len(args) > 2 and args[-1].isdigit() else 5
            count = max(1, min(count, 20))
            query = " ".join(args[1:-1]) if len(args) > 2 and args[-1].isdigit() else " ".join(args[1:])
            payload = run(args[0], query, count)
    except Exception as e:
        payload = {"ok": False, "error": str(e)}
    print(json.dumps(payload, ensure_ascii=False))
    sys.exit(0)


if __name__ == "__main__":
    main()