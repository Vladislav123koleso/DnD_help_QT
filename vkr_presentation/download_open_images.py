from __future__ import annotations

import json
import pathlib
import re
import urllib.parse
import urllib.request

OUT_DIR = pathlib.Path(__file__).resolve().parent / "open_images"
OUT_DIR.mkdir(parents=True, exist_ok=True)

QUERIES = [
    "tabletop role playing dice",
    "board game table",
    "fantasy map",
    "dungeon map",
    "software developer desk",
    "computer user interface",
    "data chart",
    "team meeting",
    "laptop workstation",
    "software architecture diagram",
    "project management board",
    "keyboard coding",
]


def slug(s: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", s.lower()).strip("-")


def fetch_json(url: str) -> dict:
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode("utf-8", errors="ignore"))


def fetch_bytes(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def main() -> None:
    seen = set()
    idx = 1
    credits = []

    for q in QUERIES:
        url = (
            "https://commons.wikimedia.org/w/api.php?"
            "action=query&generator=search&gsrnamespace=6&gsrlimit=20&"
            "prop=imageinfo&iiprop=url|extmetadata&iiurlwidth=1920&format=json&"
            f"gsrsearch={urllib.parse.quote(q)}"
        )
        data = fetch_json(url)
        pages = (data.get("query") or {}).get("pages") or {}
        for _, it in pages.items():
            if idx > 14:
                break

            imageinfo = (it.get("imageinfo") or [{}])[0]
            img_url = imageinfo.get("thumburl") or imageinfo.get("url")
            if not img_url or img_url in seen:
                continue
            seen.add(img_url)

            ext = pathlib.Path(urllib.parse.urlparse(img_url).path).suffix.lower()
            if ext not in {".jpg", ".jpeg", ".png", ".webp"}:
                ext = ".jpg"

            fname = f"img_{idx:02d}_{slug(q)[:22]}{ext}"
            out = OUT_DIR / fname

            try:
                content = fetch_bytes(img_url)
            except Exception:
                continue

            if len(content) < 10_000:
                continue

            # basic signature check
            if not (
                content.startswith(b"\xff\xd8")
                or content.startswith(b"\x89PNG")
                or content.startswith(b"RIFF")
            ):
                continue

            out.write_bytes(content)
            extmeta = imageinfo.get("extmetadata") or {}
            lic = (extmeta.get("LicenseShortName") or {}).get("value", "")
            lic_url = (extmeta.get("LicenseUrl") or {}).get("value", "")
            artist = (extmeta.get("Artist") or {}).get("value", "")
            credits.append(
                {
                    "file": fname,
                    "title": it.get("title", ""),
                    "provider": "Wikimedia Commons",
                    "license": lic,
                    "license_url": lic_url,
                    "creator": artist,
                    "source": imageinfo.get("descriptionurl") or img_url,
                }
            )
            idx += 1
        if idx > 14:
            break

    (OUT_DIR / "credits.json").write_text(
        json.dumps(credits, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(f"Downloaded: {idx-1} images")


if __name__ == "__main__":
    main()
