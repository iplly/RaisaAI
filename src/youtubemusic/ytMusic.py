#!/usr/bin/env python3
"""Мост Raisa -> YouTube Music.

CLI, всегда печатает один JSON в stdout и завершается с кодом 0:

    python3 ytMusic.py search <запрос>        -> {"ok": true, "tracks": [...]}
    python3 ytMusic.py stream <videoId> [quality] -> {"ok": true, "url": "..."}  # quality: low|medium|high, default medium
    python3 ytMusic.py similar <videoId>      -> {"ok": true, "tracks": [...]}  # радио, похожие треки

Любая ошибка -> {"ok": false, "error": "..."} (процесс НЕ падает).
"""

import json
import os
import sys
import time

import urllib3.util.connection

# IPv6 на этой машине битый: без этого каждый запрос виснет на таймаут,
# пока urllib3 не упадёт с AAAA на IPv4.
urllib3.util.connection.HAS_IPV6 = False

try:
    from ytmusicapi import YTMusic

    if os.path.exists("oauth.json"):
        yt = YTMusic("oauth.json")
    else:
        yt = YTMusic()
except Exception as e:
    print(
        json.dumps(
            {"ok": False, "error": f"ytmusicapi не доступен: {e}"}, ensure_ascii=False
        )
    )
    sys.exit(0)


def search(query):
    last_err = None
    for attempt in range(2):
        try:
            results = yt.search(query, filter="songs", limit=5)
            tracks = []
            for item in results:
                video_id = item.get("videoId")
                if not video_id:
                    continue
                tracks.append(
                    {
                        "id": video_id,
                        "title": item.get("title"),
                        "duration": item.get("duration"),
                        "artist": ", ".join(
                            a["name"] for a in item.get("artists", []) if a.get("name")
                        ),
                    }
                )
            return {"ok": True, "tracks": tracks}
        except Exception as e:
            last_err = e
            time.sleep(1 + attempt)
    return {"ok": False, "error": str(last_err)}


def format_rank(fmt):
    quality = fmt.get("audioQuality", "").rsplit("_", 1)[-1]
    return {"HIGH": 2, "MEDIUM": 1, "LOW": 0}.get(quality, -1)


QUALITIES = ("low", "medium", "high")


def norm_quality(quality):
    return quality if quality in QUALITIES else "medium"


def _stream_via_ytmusic(video_id, target="medium"):
    target_rank = format_rank({"audioQuality": target.upper()})
    song = yt.get_song(video_id)
    streaming = song.get("streamingData", {})
    formats = streaming.get("adaptiveFormats", []) + streaming.get("formats", [])
    audio = [f for f in formats if f.get("audioOnly") and f.get("url")]
    if not audio:
        audio = [
            f
            for f in formats
            if f.get("mimeType", "").startswith("audio/") and f.get("url")
        ]
    if audio:
        ranked = [f for f in audio if format_rank(f) <= target_rank] or audio
        return max(ranked, key=format_rank)["url"]
    return None


def stream(video_id, quality="medium"):
    quality = norm_quality(quality)
    last_err = None
    for attempt in range(3):
        try:
            url = _stream_via_ytmusic(video_id, quality)
            if url is None:
                last_err = "ytmusicapi не вернул аудио-форматов"
                break
            return {"ok": True, "url": url}
        except Exception as e:
            last_err = e
        time.sleep(1 + attempt)
    res = stream_via_ytdlp(video_id, quality)
    if res.get("ok"):
        return res
    res["error"] = f"{res['error']} (ytmusicapi: {last_err})"
    return res


def stream_via_ytdlp(video_id, quality="medium"):
    try:
        import yt_dlp

        formats = {
            "low": "worstaudio/bestaudio",
            "medium": "bestaudio[abr<=128]/bestaudio",
            "high": "bestaudio/best",
        }
        opts = {
            "format": formats[norm_quality(quality)],
            "quiet": True,
            "no_warnings": True,
            "noplaylist": True,
            "retries": 3,
            "socket_timeout": 20,
        }
        if os.path.exists("cookies.txt"):
            opts["cookiefile"] = "cookies.txt"
        with yt_dlp.YoutubeDL(opts) as ydl:
            info = ydl.extract_info(
                f"https://music.youtube.com/watch?v={video_id}", download=False
            )
            url = info.get("url")
            if not url:
                for f in info.get("formats", []):
                    if (
                        f.get("vcodec") == "none"
                        and f.get("acodec") != "none"
                        and f.get("url")
                    ):
                        url = f["url"]
                        break
            if not url:
                return {"ok": False, "error": f"yt-dlp не дал аудио для {video_id}"}
            return {"ok": True, "url": url}
    except Exception as e:
        return {"ok": False, "error": str(e)}


def similar(video_id, limit=5):
    last_err = None
    for attempt in range(2):
        try:
            watch = yt.get_watch_playlist(video_id, radio=True, limit=limit)
            tracks = []
            for item in watch.get("tracks", []):
                vid = item.get("videoId")
                if not vid or vid == video_id:
                    continue
                tracks.append(
                    {
                        "id": vid,
                        "title": item.get("title"),
                        "duration": item.get("length"),
                        "artist": ", ".join(
                            a["name"] for a in item.get("artists", []) if a.get("name")
                        ),
                    }
                )
            return {"ok": True, "tracks": tracks[:limit]}
        except Exception as e:
            last_err = e
            time.sleep(1 + attempt)
    return {"ok": False, "error": str(last_err)}


def main():
    args = sys.argv[1:]
    if len(args) < 2:
        payload = {
            "ok": False,
            "error": (
                f"usage: {sys.argv[0]} search <query> | stream <videoId> "
                f"[low|medium|high] | similar <videoId> [count]"
            ),
        }
    elif args[0] == "search":
        payload = search(args[1])
    elif args[0] == "stream":
        quality = norm_quality(args[2]) if len(args) > 2 else "medium"
        payload = stream(args[1], quality)
    elif args[0] == "similar":
        limit = int(args[2]) if len(args) > 2 else 5
        payload = similar(args[1], limit)
    else:
        payload = {"ok": False, "error": f"неизвестная команда: {args[0]}"}
    print(json.dumps(payload, ensure_ascii=False))
    sys.exit(0)


if __name__ == "__main__":
    main()
