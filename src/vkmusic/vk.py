#!/usr/bin/env python3
"""Мост Raisa -> VK Music (веб-API api.vk.ru, токен из vk.conf).

Токен vk1.a.* живёт ~4 часа, продлевается как делает браузер:
POST login.vk.ru/?act=web_token с куками (remixsid, p) -> новый access_token.
vk.py продлевает токен ПЕРЕД каждым вызовом и использует свежий локально.
В vk.conf сохраняет только команда refresh (для cron): так параллельные
вызовы не дерутся за запись файла. Если mint падает (сессия умерла) —
печатается ошибка с подсказкой перелогиниться.

CLI, всегда печатает один JSON в stdout и завершается с кодом 0:

    python3 vk.py search <запрос>        -> {"ok": true, "tracks": [...]}
        Если в запросе есть «микс»  -> вызывает audio.getStreamMixAudios
        Если в запросе есть «плейлист» -> открывает VK_PLAYLIST_ENTITY из vk.conf
    python3 vk.py mix                    -> {"ok": true, "tracks": [...]}   # 3 трека; повторный вызов = продолжение микса
    python3 vk.py playlist <id>          -> {"ok": true, "tracks": [...]}   # id: только номер плейлиста (uid/access_key подхватятся сами: положительный = твой, отрицательный = сгенерированный алгоритмом); допустима и полная сущность <owner>_<id>_<key>
    python3 vk.py similar <id> [count]   -> похожие треки по seed (audio.getStreamMixAudios); id: <owner>_<track>
    python3 vk.py stream <id>            -> {"ok": true, "url": "..."}      # свежий m3u8; id: <owner>_<track>_<access_key>
    python3 vk.py refresh                -> продлить токен и сохранить свежий в vk.conf (для cron/демона; единственная команда, которая пишет файл)
    python3 vk.py myplaylists            -> {"ok": true, "playlists": [...]} # мои плейлисты: entity/title/count/is_following
    python3 vk.py my [count]             -> {"ok": true, "tracks": [...]}    # мои треки (по умолчанию 20)
    python3 vk.py playplaylist <название>-> {"ok": true, "tracks": [...]}    # треки моего плейлиста по названию
    python3 vk.py generated              -> {"ok": true, "playlists": [...]} # сгенерированные алгоритмами (отрицательные id)
    python3 vk.py playgenerated <название|id> -> {"ok": true, "tracks": [...]} # треки сгенерированного плейлиста

uid определяется каждый вызов через users.get — команды не привязаны к одному
аккаунту: какой аккаунт сейчас в сессии (токен/куки в vk.conf), тот и войдёт.

Любая ошибка -> {"ok": false, "error": "..."} (процесс НЕ падает).

vk.conf (в текущей директории, как oauth.json у ytMusic.py):
    VK_ACCESS_TOKEN=vk1.a....     # обновляется командой refresh (cron); вызовы минтит свежий сами, но файл не пишут
    VK_COOKIE=...                 # куки из браузера: remixsid=...; p=...; (httoken обновляется при refresh)
    VK_PLAYLIST_ENTITY=...        # <owner>_<playlist>_<access_key> для «включи плейлист»
"""

import json
import os
import sys
import time
import urllib.parse

import urllib3
import urllib3.util.connection

# IPv6 на этой машине битый: без этого каждый запрос виснет на таймаут,
# пока urllib3 не упадёт с AAAA на IPv4.
urllib3.util.connection.HAS_IPV6 = False

API = "https://api.vk.ru/method"
V = "5.285"
CLIENT_ID = "6287487"
HEADERS = {
    "User-Agent": "Mozilla/5.0 (X11; Linux x86_64; rv:153.0) Gecko/20100101 Firefox/153.0",
    "Content-Type": "application/x-www-form-urlencoded",
    "Referer": "https://vk.ru/",
}


def load_config():
    cfg = {}
    if os.path.exists("vk.conf"):
        with open("vk.conf", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, _, v = line.partition("=")
                cfg[k.strip()] = v.strip()
    return cfg


def save_config(cfg):
    with open("vk.conf", "w", encoding="utf-8") as f:
        f.write("# VK_ACCESS_TOKEN обновляется командой refresh (vk.py refresh)\n")
        for k, v in cfg.items():
            f.write(f"{k}={v}\n")


def set_cookie(cookie_str, name, value):
    parts = []
    found = False
    for part in cookie_str.split(";"):
        part = part.strip()
        if not part:
            continue
        if part.startswith(name + "="):
            parts.append(f"{name}={value}")
            found = True
        else:
            parts.append(part)
    if not found:
        parts.append(f"{name}={value}")
    return "; ".join(parts)


def mint_token(http, cookie):
    url = "https://login.vk.ru/?act=web_token"
    headers = {
        "User-Agent": "Mozilla/5.0 (X11; Linux x86_64; rv:153.0) Gecko/20100101 Firefox/153.0",
        "Accept": "*/*",
        "Accept-Language": "ru-RU,ru;q=0.9,en-US;q=0.8,en;q=0.7",
        "Content-Type": "application/x-www-form-urlencoded",
        "Referer": "https://vk.ru/",
        "Origin": "https://vk.ru",
        "Cookie": cookie,
    }
    last_err = None
    for attempt in range(2):
        try:
            r = http.request(
                "POST", url, body="version=1&app_id=6287487", headers=headers
            )
            data = json.loads(r.data.decode("utf-8", "replace"))
        except Exception as e:
            last_err = e
            time.sleep(1 + attempt)
            continue
        if data.get("type") != "okay":
            return {
                "ok": False,
                "error": f"web_token: {data.get('type')} {data.get('error_info')}",
            }
        token = data.get("data", {}).get("access_token")
        if not token:
            return {"ok": False, "error": "web_token: пустой access_token"}
        new_httoken = None
        for sc in r.headers.getlist("Set-Cookie") or []:
            head = sc.split(";", 1)[0].strip()
            if head.startswith("httoken="):
                new_httoken = head.split("=", 1)[1]
        return {"ok": True, "token": token, "httoken": new_httoken}
    return {"ok": False, "error": str(last_err) or "сеть недоступна"}


def api_call(http, method, params, retries=2):
    url = f"{API}/{method}?v={V}&client_id={CLIENT_ID}"
    body = urllib.parse.urlencode(params)
    last_err = None
    for attempt in range(retries):
        try:
            r = http.request("POST", url, body=body, headers=HEADERS)
            data = json.loads(r.data.decode("utf-8", "replace"))
        except Exception as e:
            last_err = e
            time.sleep(1 + attempt)
            continue
        err = data.get("error")
        if err:
            return {
                "ok": False,
                "error": f"VK {err.get('error_code')}: {err.get('error_msg')}",
            }
        return {"ok": True, "data": data}
    return {"ok": False, "error": str(last_err) or "сеть недоступна"}


def to_track(t):
    owner = t.get("owner_id")
    tid = t.get("id")
    if not owner or not tid:
        return None
    key = t.get("access_key") or ""
    track_id = f"{owner}_{tid}" if not key else f"{owner}_{tid}_{key}"
    return {
        "id": track_id,
        "title": t.get("title"),
        "artist": t.get("artist"),
        "duration": t.get("duration"),
    }


def tracks_of(data):
    resp = data.get("response")
    if not isinstance(resp, list):
        return []
    return [t for t in (to_track(a) for a in resp) if t]


def resolve_uid(http, token):
    res = api_call(http, "users.get", {"access_token": token})
    if not res["ok"]:
        return None
    resp = res["data"].get("response")
    if isinstance(resp, list) and resp and resp[0].get("id"):
        return resp[0]["id"]
    return None


def cmd_myplaylists(http, token):
    uid = resolve_uid(http, token)
    if not uid:
        return {"ok": False, "error": "не удалось определить текущего пользователя"}
    res = api_call(
        http,
        "audio.getPlaylists",
        {"owner_id": uid, "count": 200, "need_blocks": 1, "access_token": token},
    )
    if not res["ok"]:
        return res
    items = res["data"].get("response", {}).get("items") or []
    playlists = []
    for p in items:
        pid = p.get("id")
        if not pid:
            continue
        playlists.append(
            {
                "id": pid,
                "entity": f"{uid}_{pid}_{p.get('access_key', '')}".rstrip("_"),
                "title": p.get("title"),
                "count": p.get("count"),
                "type": p.get("type"),
                "is_following": bool(p.get("is_following")),
            }
        )
    if not playlists:
        return {"ok": False, "error": "плейлисты не найдены"}
    return {"ok": True, "playlists": playlists}


def cmd_my(http, token, count=20):
    uid = resolve_uid(http, token)
    if not uid:
        return {"ok": False, "error": "не удалось определить текущего пользователя"}
    res = api_call(
        http,
        "audio.get",
        {
            "owner_id": uid,
            "need_user": 0,
            "count": count,
            "access_token": token,
        },
    )
    if not res["ok"]:
        return res
    items = res["data"].get("response", {}).get("items") or []
    tracks = [t for t in (to_track(a) for a in items) if t]
    if not tracks:
        return {"ok": False, "error": "в твоей медиатеке нет треков"}
    return {"ok": True, "tracks": tracks}


def cmd_playplaylist(http, token, title):
    uid = resolve_uid(http, token)
    if not uid:
        return {"ok": False, "error": "не удалось определить текущего пользователя"}
    res = api_call(
        http,
        "audio.getPlaylists",
        {"owner_id": uid, "count": 200, "need_blocks": 1, "access_token": token},
    )
    if not res["ok"]:
        return res
    items = res["data"].get("response", {}).get("items") or []
    needle = title.strip().lower()
    match = None
    for p in items:
        ptitle = (p.get("title") or "").strip().lower()
        if ptitle.startswith(needle) or needle in ptitle:
            match = p
            break
    if not match or not match.get("id"):
        names = ", ".join(f"«{p.get('title')}»" for p in items if p.get("title")) or "(пусто)"
        return {
            "ok": False,
            "error": f"плейлист «{title}» не найден. Доступно: {names}",
        }
    entity = f"{uid}_{match['id']}_{match.get('access_key', '')}".rstrip("_")
    return cmd_playlist(http, token, entity)


def cmd_search(http, token, query):
    res = api_call(
        http,
        "catalog.getAudioSearch",
        {
            "query": query,
            "need_blocks": 1,
            "screen_ref": "search_music_service",
            "access_token": token,
        },
    )
    if not res["ok"]:
        return res
    audios = res["data"].get("response", {}).get("audios") or []
    tracks = [t for t in (to_track(a) for a in audios) if t]
    if not tracks:
        return {"ok": False, "error": "ничего не найдено"}
    return {"ok": True, "tracks": tracks}


def cmd_mix(http, token):
    res = api_call(
        http, "audio.getStreamMixAudios", {"mix_id": "common", "access_token": token}
    )
    if not res["ok"]:
        return res
    tracks = tracks_of(res["data"])
    if not tracks:
        return {"ok": False, "error": "пустой ответ микса"}
    return {"ok": True, "tracks": tracks}


def cmd_similar(http, token, audio_id, count=20):
    res = api_call(
        http,
        "audio.getStreamMixAudios",
        {"mix_id": audio_id, "count": count, "access_token": token},
    )
    if not res["ok"]:
        return res
    tracks = tracks_of(res["data"])
    if not tracks:
        return {"ok": False, "error": "пустой ответ похожих треков"}
    return {"ok": True, "tracks": tracks}


def resolve_playlist_entity(http, token, ref):
    """Возвращает (entity, title) по ссылке:
    - полная сущность '<owner>_<id>_<key>' -> как есть
    - просто id (положительный или отрицательный) -> uid и access_key
      получаются сами: uid из users.get, ключ из audio.getPlaylistById.
    """
    ref = ref.strip()
    if "_" in ref:
        return ref, None
    if not ref.lstrip("-").isdigit():
        raise ValueError(f"плейлист должен быть числом или <owner>_<id>_<key>: {ref!r}")
    uid = resolve_uid(http, token)
    if not uid:
        raise ValueError("не удалось определить текущего пользователя")
    pid = int(ref)
    res = api_call(
        http,
        "audio.getPlaylistById",
        {"owner_id": uid, "playlist_id": pid, "count": 0, "access_token": token},
    )
    if not res["ok"]:
        raise ValueError(f"плейлист {pid} недоступен: {res['error']}")
    resp = res["data"].get("response") or {}
    ak = resp.get("access_key") or ""
    title = resp.get("title")
    return f"{uid}_{pid}_{ak}".rstrip("_"), title


def cmd_playlist(http, token, ref):
    try:
        entity_id, title = resolve_playlist_entity(http, token, ref)
    except ValueError as e:
        return {"ok": False, "error": str(e)}
    res = api_call(
        http,
        "audio.getIdsBySource",
        {
            "source": "playlist",
            "entity_id": entity_id,
            "access_token": token,
        },
    )
    if not res["ok"]:
        return res
    items = res["data"].get("response", {}).get("audios") or []
    ids = [i["audio_id"] for i in items if i.get("audio_id")]
    if not ids:
        return {"ok": False, "error": "плейлист пуст"}
    tracks = []
    for i in range(0, len(ids), 50):
        res = api_call(
            http,
            "audio.getById",
            {"audios": ",".join(ids[i : i + 50]), "access_token": token},
        )
        if not res["ok"]:
            return res
        tracks.extend(tracks_of(res["data"]))
    if not tracks:
        return {"ok": False, "error": "не удалось получить треки плейлиста"}
    payload = {"ok": True, "tracks": tracks}
    if title:
        payload["title"] = title
    return payload


GENERATED_IDS = list(range(-21, -46, -1)) + [-50, -60, -70, -90, -100]


def cmd_generated(http, token):
    uid = resolve_uid(http, token)
    if not uid:
        return {"ok": False, "error": "не удалось определить текущего пользователя"}
    playlists = []
    for pid in GENERATED_IDS:
        res = api_call(
            http,
            "audio.getPlaylistById",
            {"owner_id": uid, "playlist_id": pid, "count": 0, "access_token": token},
        )
        if not res["ok"]:
            continue
        resp = res["data"].get("response") or {}
        if not resp.get("id"):
            continue
        playlists.append(
            {
                "id": pid,
                "entity": f"{uid}_{pid}_{resp.get('access_key', '')}".rstrip("_"),
                "title": resp.get("title"),
                "count": resp.get("count"),
                "type": "generated",
            }
        )
    if not playlists:
        return {"ok": False, "error": "сгенерированные плейлисты не найдены"}
    return {"ok": True, "playlists": playlists}


def cmd_playgenerated(http, token, query):
    data = cmd_generated(http, token)
    if not data["ok"]:
        return data
    pls = data["playlists"]
    q = query.strip()
    match = None
    if q.lstrip("-").isdigit():
        nid = int(q)
        for p in pls:
            if p["id"] == nid:
                match = p
                break
    else:
        needle = q.lower()
        for p in pls:
            ptitle = (p.get("title") or "").lower()
            if ptitle.startswith(needle) or needle in ptitle:
                match = p
                break
    if not match:
        names = ", ".join(f"«{p['title']}»" for p in pls) or "(пусто)"
        return {
            "ok": False,
            "error": f"сгенерированный плейлист «{query}» не найден. Доступно: {names}",
        }
    return cmd_playlist(http, token, match["entity"])


def cmd_stream(http, token, audio_id):
    res = api_call(
        http, "audio.getById", {"audios": audio_id, "access_token": token}
    )
    if not res["ok"]:
        return res
    got = res["data"].get("response")
    if not isinstance(got, list) or not got or not got[0].get("url"):
        return {"ok": False, "error": "трек недоступен"}
    t = got[0]
    return {
        "ok": True,
        "url": t["url"],
        "title": t.get("title"),
        "artist": t.get("artist"),
    }


def main():
    cfg = load_config()
    token = cfg.get("VK_ACCESS_TOKEN")
    cookie = cfg.get("VK_COOKIE")
    if not token or not cookie:
        print(
            json.dumps(
                {"ok": False, "error": "нет VK_ACCESS_TOKEN/VK_COOKIE в vk.conf"},
                ensure_ascii=False,
            )
        )
        sys.exit(0)

    http = urllib3.PoolManager(timeout=20, retries=False)

    mint = mint_token(http, cookie)
    if mint["ok"]:
        token = mint["token"]
        cfg["VK_ACCESS_TOKEN"] = token
        if mint["httoken"]:
            cfg["VK_COOKIE"] = set_cookie(cookie, "httoken", mint["httoken"])
    else:
        print(
            json.dumps(
                {
                    "ok": False,
                    "error": (
                        f"VK сессия умерла: {mint['error']}. Перелогинься в "
                        "браузере — vk.py подхватит куки сам (расширение Raisa)"
                    ),
                },
                ensure_ascii=False,
            )
        )
        sys.exit(0)

    args = sys.argv[1:]

    if not args or args[0] not in ("search", "mix", "playlist", "similar", "stream",
                                   "refresh", "myplaylists", "my", "playplaylist",
                                   "generated", "playgenerated"):
        payload = {
            "ok": False,
            "error": (
                f"usage: {sys.argv[0]} search <query> | mix | "
                f"playlist <id> | playlist <owner>_<id>_<access_key> | "
                f"similar <id> | stream <owner>_<track>_<access_key> | "
                f"refresh | myplaylists | my [count] | playplaylist <название> | "
                f"generated | playgenerated <название|id>"
            ),
        }
    elif args[0] == "search" and len(args) < 2:
        payload = {"ok": False, "error": "нужен запрос: search <query>"}
    elif args[0] == "playlist" and len(args) < 2:
        payload = {
            "ok": False,
            "error": "нужен id плейлиста: playlist <id> (uid/access_key подхватятся сами)",
        }
    elif args[0] == "stream" and len(args) < 2:
        payload = {
            "ok": False,
            "error": "нужен id: stream <owner>_<track>_<access_key>",
        }
    elif args[0] == "similar" and len(args) < 2:
        payload = {
            "ok": False,
            "error": "нужен id: similar <owner>_<track>_<access_key>",
        }
    elif args[0] == "similar" and len(args) > 2 and not args[2].isdigit():
        payload = {"ok": False, "error": "similar принимает только число: similar <id> [count]"}
    elif args[0] == "my" and len(args) > 1 and not args[1].isdigit():
        payload = {"ok": False, "error": "my принимает только число: my [count]"}
    elif args[0] == "playplaylist" and len(args) < 2:
        payload = {"ok": False, "error": "нужно название: playplaylist <название>"}
    elif args[0] == "playgenerated" and len(args) < 2:
        payload = {"ok": False, "error": "нужно название: playgenerated <название|id>"}
    else:
        try:
            if args[0] == "search":
                query = args[1]
                lower = query.lower()
                if "микс" in lower:
                    payload = cmd_mix(http, token)
                elif "плейлист" in lower:
                    entity = cfg.get("VK_PLAYLIST_ENTITY")
                    if not entity:
                        payload = {
                            "ok": False,
                            "error": "нет VK_PLAYLIST_ENTITY в vk.conf",
                        }
                    else:
                        payload = cmd_playlist(http, token, entity)
                else:
                    payload = cmd_search(http, token, query)
            elif args[0] in ("mix",):
                payload = cmd_mix(http, token)
            elif args[0] == "similar":
                count = int(args[2]) if len(args) > 2 else 20
                count = max(0, min(count, 500))
                payload = cmd_similar(http, token, args[1], count)
            elif args[0] == "refresh":
                try:
                    save_config(cfg)
                except OSError:
                    pass
                payload = {"ok": True, "token": token[:20] + "..."}
            elif args[0] == "playlist":
                payload = cmd_playlist(http, token, args[1])
            elif args[0] == "myplaylists":
                payload = cmd_myplaylists(http, token)
            elif args[0] == "my":
                count = int(args[1]) if len(args) > 1 else 20
                count = max(0, min(count, 1000))
                payload = cmd_my(http, token, count)
            elif args[0] == "playplaylist":
                payload = cmd_playplaylist(http, token, " ".join(args[1:]))
            elif args[0] == "generated":
                payload = cmd_generated(http, token)
            elif args[0] == "playgenerated":
                payload = cmd_playgenerated(http, token, " ".join(args[1:]))
            else:
                payload = cmd_stream(http, token, args[1])
        except Exception as e:
            payload = {"ok": False, "error": str(e)}

    print(json.dumps(payload, ensure_ascii=False))
    sys.exit(0)


if __name__ == "__main__":
    main()