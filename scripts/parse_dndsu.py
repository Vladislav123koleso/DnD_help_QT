from pathlib import Path
import json
import re
import time
from urllib.parse import urljoin, urlparse

import requests
from bs4 import BeautifulSoup


BASE_URL = "https://dnd.su"
RACES_URL = "https://dnd.su/race/"
CLASSES_URL = "https://dnd.su/class/"
BACKGROUNDS_URL = "https://dnd.su/backgrounds/"
FEATS_URL = "https://dnd.su/feats/"
REPO_ROOT = Path(__file__).resolve().parents[1]

HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36"
    )
}

CLASS_PRIMARY_ABILITIES = {
    "Бард": "Харизма",
    "Варвар": "Сила",
    "Воин": "Сила или Ловкость",
    "Волшебник": "Интеллект",
    "Друид": "Мудрость",
    "Жрец": "Мудрость",
    "Изобретатель": "Интеллект",
    "Колдун": "Харизма",
    "Монах": "Ловкость и Мудрость",
    "Паладин": "Сила и Харизма",
    "Плут": "Ловкость",
    "Следопыт": "Ловкость и Мудрость",
    "Чародей": "Харизма",
    "Боец": "Сила или Ловкость",
    "Заклинатель": "Зависит от роли",
    "Эксперт": "Зависит от роли",
}

CLASS_DESCRIPTIONS = {
    "Бард": "Универсальный заклинатель, сочетающий поддержку, контроль и сильные навыки вне боя.",
    "Варвар": "Фронтовой воин, полагающийся на ярость, выживаемость и высокий урон в ближнем бою.",
    "Воин": "Гибкий боевой класс, одинаково уверенно работающий с оружием, доспехами и тактикой.",
    "Волшебник": "Подготовленный заклинатель с самой широкой книгой заклинаний и сильным контролем поля боя.",
    "Друид": "Заклинатель природы с доступом к контролю, лечению и тематическим природным способностям.",
    "Жрец": "Божественный заклинатель, сочетающий поддержку, защиту, лечение и доменные умения.",
    "Изобретатель": "Интеллектуальный полузаклинатель, усиливающий группу инфузиями, магическими предметами и изобретениями.",
    "Колдун": "Заклинатель, чья сила строится на покровителе, воззваниях и ограниченном, но мощном наборе ячеек.",
    "Монах": "Мобильный боец, использующий ки, боевые искусства и высокую скорость для контроля дистанции.",
    "Паладин": "Священный воин с аурами, исцелением, защитой группы и мощными карающими ударами.",
    "Плут": "Манёвренный специалист по скрытности, навыкам и точечному урону скрытой атакой.",
    "Следопыт": "Следопыт и охотник, сочетающий боевые умения, выживание и ограниченную природную магию.",
    "Чародей": "Спонтанный заклинатель, усиливающий магию метамагией и опирающийся на врождённый источник силы.",
    "Боец": "Спутник-воитель с простой прогрессией, рассчитанный на базовую боевую поддержку группы.",
    "Заклинатель": "Спутник-заклинатель с упрощённой магической прогрессией и ограниченным набором ролей.",
    "Эксперт": "Спутник-специалист, ориентированный на проверки, помощь группе и навыковую полезность.",
}

NUMBER_WORDS = {
    "0": 0,
    "ноль": 0,
    "1": 1,
    "один": 1,
    "одна": 1,
    "одну": 1,
    "2": 2,
    "два": 2,
    "две": 2,
    "3": 3,
    "три": 3,
    "4": 4,
    "четыре": 4,
    "5": 5,
    "пять": 5,
    "6": 6,
    "шесть": 6,
}

LANGUAGE_NORMALIZATION = {
    "общем": "Общий",
    "общий": "Общий",
    "гитском": "Гитский",
    "гитский": "Гитский",
    "гоблинском": "Гоблинский",
    "гоблинский": "Гоблинский",
    "дварфийском": "Дварфийский",
    "дварфийский": "Дварфийский",
    "эльфийском": "Эльфийский",
    "эльфийский": "Эльфийский",
    "гигантском": "Великаний",
    "великаний": "Великаний",
    "драконьем": "Драконий",
    "драконий": "Драконий",
    "инфернальном": "Инфернальный",
    "инфернальный": "Инфернальный",
    "небесном": "Небесный",
    "небесный": "Небесный",
    "сильване": "Сильван",
    "сильван": "Сильван",
    "акване": "Акван",
    "акван": "Акван",
    "ауране": "Ауран",
    "ауран": "Ауран",
    "игнане": "Игнан",
    "игнан": "Игнан",
    "терране": "Терран",
    "терран": "Терран",
    "первичном": "Первичный",
    "первичный": "Первичный",
    "подземном": "Подземный",
    "подземный": "Подземный",
    "орочьем": "Орочий",
    "орочий": "Орочий",
    "гномьем": "Гномий",
    "гномий": "Гномий",
    "леонинов": "Леонинский",
    "леонинский": "Леонинский",
    "бездны": "Язык Бездны",
}

STAT_ALIASES = {
    "str": ["Сила", "Силы", "Силу"],
    "dex": ["Ловкость", "Ловкости", "Ловкостью"],
    "con": ["Телосложение", "Телосложения", "Телосложением"],
    "int": ["Интеллект", "Интеллекта", "Интеллектом"],
    "wis": ["Мудрость", "Мудрости", "Мудростью"],
    "cha": ["Харизма", "Харизмы", "Харизмой"],
}

STOP_HEADINGS = ("Комментарии", "Галерея")
IGNORE_TRAIT_HEADERS = (
    "Источник",
    "Галерея",
    "Комментарии",
    "Показать email",
    "Распечатать",
)


def elf_asi(dexterity, intelligence=0, wisdom=0, charisma=0, strength=0):
    return {
        "str": strength,
        "dex": dexterity,
        "con": 0,
        "int": intelligence,
        "wis": wisdom,
        "cha": charisma,
    }


def base_elf_traits():
    return {
        "Увеличение характеристик": "Ловкость +2.",
        "Тёмное зрение": "Вы видите в темноте на 60 футов как при тусклом освещении и различаете только оттенки серого.",
        "Обострённые чувства": "Вы владеете навыком Восприятие.",
        "Наследие фей": "Вы совершаете с преимуществом спасброски против состояния очарованный, и магия не может вас усыпить.",
        "Транс": "Вместо сна вы медитируете 4 часа и получаете преимущества продолжительного отдыха.",
        "Языки": "Вы говорите, читаете и пишете на Общем и Эльфийском.",
    }


def make_legacy_elf_variant(base_race, slug, name, source, description, asi, extra_traits, languages=None):
    race = {
        "slug": slug,
        "name": name,
        "source": source,
        "description": description,
        "speed": 30,
        "flyingSpeed": 0,
        "size": "Средний",
        "asi": asi,
        "traits": {**base_elf_traits(), **extra_traits},
        "languages": list(languages or ["Общий", "Эльфийский"]),
    }
    if base_race.get("imagePath"):
        race["imagePath"] = base_race["imagePath"]
    return race


def normalize_race_entries(races):
    normalized = []
    seen_names = set()
    elf_source = None

    for race in races:
        if race.get("name") == "Эльф":
            elf_source = dict(race)
            cleaned = dict(race)
            cleaned["source"] = "Player's Handbook"
            cleaned["asi"] = elf_asi(2)
            cleaned["speed"] = 30
            cleaned["flyingSpeed"] = 0
            cleaned["size"] = "Средний"
            cleaned["traits"] = base_elf_traits()
            cleaned["languages"] = ["Общий", "Эльфийский"]
            normalized.append(cleaned)
            seen_names.add(cleaned["name"])
            continue

        normalized.append(race)
        seen_names.add(race.get("name", ""))

    if not elf_source:
        return normalized

    variants = [
        make_legacy_elf_variant(
            elf_source,
            "race/79-high-elf",
            "Высший эльф",
            "Player's Handbook",
            "Высшие эльфы развивают магию и изящное владение традиционным оружием.",
            elf_asi(2, intelligence=1),
            {
                "Увеличение характеристик": "Ловкость +2, Интеллект +1.",
                "Владение эльфийским оружием": "Вы владеете длинным мечом, коротким мечом, длинным луком и коротким луком.",
                "Заговор": "Вы знаете один заговор из списка волшебника. Базовая характеристика для него — Интеллект.",
                "Дополнительный язык": "Вы знаете ещё один язык по выбору.",
            },
            ["Общий", "Эльфийский", "Дополнительный язык по выбору"],
        ),
        make_legacy_elf_variant(
            elf_source,
            "race/79-wood-elf",
            "Лесной эльф",
            "Player's Handbook",
            "Лесные эльфы быстрее других эльфов и лучше скрываются среди природных укрытий.",
            elf_asi(2, wisdom=1),
            {
                "Увеличение характеристик": "Ловкость +2, Мудрость +1.",
                "Быстрые ноги": "Ваша скорость ходьбы увеличивается до 35 футов.",
                "Маскировка в дикой местности": "Вы можете прятаться, если вас слабо скрывают листва, снег, дождь, туман и другие природные явления.",
            },
        ),
        make_legacy_elf_variant(
            elf_source,
            "race/79-drow",
            "Дроу",
            "Player's Handbook",
            "Дроу приспособлены к Подземью, владеют особой магией и страдают от яркого солнечного света.",
            elf_asi(2, charisma=1),
            {
                "Увеличение характеристик": "Ловкость +2, Харизма +1.",
                "Превосходное тёмное зрение": "Ваше тёмное зрение увеличивается до 120 футов.",
                "Чувствительность к солнцу": "При прямом солнечном свете вы получаете помеху на броски атаки и проверки Восприятия, основанные на зрении.",
                "Магия дроу": "С 3 уровня вы можете накладывать Огонь фей, а с 5 уровня — Тьму, восстанавливая использование после продолжительного отдыха.",
                "Владение оружием дроу": "Вы владеете рапирой, коротким мечом и ручным арбалетом.",
            },
        ),
        make_legacy_elf_variant(
            elf_source,
            "race/79-grugach",
            "Гругач",
            "Mordenkainen's Tome of Foes",
            "Гругачи — дикие эльфы, полагающиеся на силу, оружейную выучку и простую магию природы.",
            elf_asi(2, strength=1),
            {
                "Увеличение характеристик": "Ловкость +2, Сила +1.",
                "Боевая подготовка гругачей": "Вы владеете копьём, коротким луком, длинным луком и сетью.",
                "Заговор": "Вы знаете один заговор из списка друида. Базовая характеристика для него — Мудрость.",
            },
        ),
    ]

    for variant in variants:
        if variant["name"] not in seen_names:
            normalized.append(variant)
            seen_names.add(variant["name"])

    return normalized
NOISE_CLASS_MARKERS = (
    "new-article-menu",
    "dropdown-list",
    "breadcrumbs",
    "comment",
    "gallery",
    "social",
    "share",
)


SESSION = requests.Session()
SESSION.headers.update(HEADERS)


def clean_text(text):
    if not text:
        return ""
    text = text.replace("\xa0", " ")
    return re.sub(r"\s+", " ", text).strip()


def dedupe_preserve_order(values):
    unique = []
    for value in values:
        cleaned = clean_text(value)
        if cleaned and cleaned not in unique:
            unique.append(cleaned)
    return unique


def fetch_soup(url):
    last_error = None
    for attempt in range(3):
        try:
            response = SESSION.get(url, timeout=30)
            response.raise_for_status()
            return BeautifulSoup(response.text, "html.parser")
        except requests.RequestException as error:
            last_error = error
            if attempt == 2:
                raise
            time.sleep(1 + attempt)

    raise last_error


def collect_links(page_url, href_pattern):
    soup = fetch_soup(page_url)
    links = []
    seen = set()

    for anchor in soup.find_all("a", href=True):
        href = anchor["href"].strip()
        if re.fullmatch(href_pattern, href):
            full_url = urljoin(BASE_URL, href)
            if full_url not in seen:
                seen.add(full_url)
                links.append(full_url)

    return links


def get_slug(url):
    return urlparse(url).path.strip("/")


def extract_name(soup):
    for heading in soup.find_all(["h1", "h2"]):
        text = clean_text(heading.get_text(" ", strip=True))
        text = re.sub(r"\s*\[[^\]]+\]\s*", "", text).strip()
        text = re.split(r"\s+[—-]\s+", text, maxsplit=1)[0].strip()
        if text and text != "DnD.su":
            return text

    if soup.title and soup.title.string:
        text = clean_text(soup.title.string)
        text = re.split(r"\s+[—-]\s+", text, maxsplit=1)[0]
        text = re.sub(r"\s*\[[^\]]+\]\s*", "", text).strip()
        if text and text != "DnD.su":
            return text

    return ""


def extract_source(soup):
    page_text = soup.get_text(" ", strip=True)
    for pattern in (
        r"Источник:\s*«([^»]+)»",
        r'Источник:\s*"([^"]+)"',
        r"Источник:\s*([^\n]+?)(?:\s+свернуть|$)",
    ):
        match = re.search(pattern, page_text)
        if match:
            return clean_text(match.group(1))

    if soup.title and soup.title.string:
        title_parts = [clean_text(part) for part in soup.title.string.split("/") if clean_text(part)]
        if title_parts:
            last_part = title_parts[-1]
            if last_part not in {"DnD.su", "D&D 5"}:
                return last_part

    return ""


def extract_description(soup, name):
    lines = [clean_text(line) for line in soup.get_text("\n", strip=True).splitlines() if clean_text(line)]
    description_lines = []
    source_seen = False

    for line in lines:
        if line.startswith("Источник:"):
            source_seen = True
            continue

        if not source_seen:
            continue

        if line in STOP_HEADINGS or line == name:
            break

        if re.fullmatch(r"[А-ЯЁA-Z0-9\s()'\"-]{4,}", line):
            if description_lines:
                break
            continue

        if len(line) < 60 or not re.search(r"[.!?]", line):
            if description_lines:
                break
            continue

        description_lines.append(line)
        if len(description_lines) >= 3 or len(" ".join(description_lines)) >= 600:
            break

    return clean_text(" ".join(description_lines))


def extract_content_root(soup):
    selectors = [
        ".card__body",
        ".card-body",
        ".article-body",
        "article",
        "main",
    ]

    for selector in selectors:
        root = soup.select_one(selector)
        if root:
            return root

    return soup


def is_stop_heading(text):
    lowered = text.lower()
    return any(stop.lower() in lowered for stop in STOP_HEADINGS)


def is_noise_tag(tag):
    for ancestor in [tag, *tag.parents]:
        classes = ancestor.get("class") or []
        identifier = ancestor.get("id") or ""
        haystack = " ".join(classes + [identifier]).lower()
        if any(marker in haystack for marker in NOISE_CLASS_MARKERS):
            return True
        if getattr(ancestor, "name", None) in {"nav", "header", "footer"}:
            return True
    return False


def extract_feature_name(tag):
    for selector in ("span.dnd-feature-name", "span.article-body__feature-name"):
        feature = tag.select_one(selector)
        if feature:
            return clean_text(feature.get_text(" ", strip=True))

    bold = tag.find(["strong", "b"])
    if bold:
        bold_text = clean_text(bold.get_text(" ", strip=True))
        if bold_text and len(bold_text) <= 80:
            return bold_text

    return ""


def strip_leading_feature_label(text, feature_name):
    pattern = rf"^{re.escape(feature_name)}\s*[.:]\s*"
    stripped = re.sub(pattern, "", text, count=1, flags=re.IGNORECASE)
    if stripped != text:
        return clean_text(stripped)
    if text.lower().startswith(feature_name.lower()):
        return clean_text(text[len(feature_name):].lstrip(" .:-"))
    return text


def commit_trait(traits, name, parts):
    text = clean_text(" ".join(parts))
    title = clean_text(name).rstrip(":.")
    if not title or not text:
        return
    if should_ignore_trait_title(title):
        return
    traits[title] = text


def should_ignore_trait_title(title):
    return title in IGNORE_TRAIT_HEADERS or title.startswith("Имена ") or title == "Подрасы"


def extract_traits(soup):
    root = extract_content_root(soup)
    direct_traits = {}

    for tag in root.find_all(["p", "li"]):
        if is_noise_tag(tag):
            continue

        text = clean_text(tag.get_text(" ", strip=True))
        if not text:
            continue

        feature_name = extract_feature_name(tag)
        if not feature_name:
            continue

        normalized_name = feature_name.rstrip(":.")
        if should_ignore_trait_title(normalized_name) or normalized_name in direct_traits:
            continue

        remainder = strip_leading_feature_label(text, feature_name)
        if not remainder or remainder == text:
            continue

        direct_traits[normalized_name] = remainder

    if direct_traits:
        return direct_traits

    traits = {}
    current_name = ""
    current_parts = []
    started = False
    wait_for_features_heading = any(
        clean_text(tag.get_text(" ", strip=True)).lower().startswith("особенности ")
        for tag in root.find_all(["h3", "h4"])
    )

    for tag in root.find_all(["h3", "h4", "p", "li"]):
        if is_noise_tag(tag):
            continue

        text = clean_text(tag.get_text(" ", strip=True))
        if not text:
            continue

        if tag.name in ("h3", "h4"):
            if is_stop_heading(text):
                break
            if wait_for_features_heading and not text.lower().startswith("особенности ") and not started:
                continue
            if started:
                commit_trait(traits, current_name, current_parts)
            current_name = ""
            if not text.lower().startswith("особенности "):
                current_name = text.replace("свернуть", "").strip()
            current_parts = []
            started = True
            continue

        if not started:
            continue

        feature_name = extract_feature_name(tag)
        if feature_name:
            normalized_feature_name = feature_name.rstrip(":.")
            remainder = strip_leading_feature_label(text, feature_name)
            if normalized_feature_name and remainder != text:
                commit_trait(traits, current_name, current_parts)
                current_name = normalized_feature_name
                current_parts = [remainder] if remainder else []
                continue

        if is_stop_heading(text):
            break

        if current_name:
            current_parts.append(text)

    if started:
        commit_trait(traits, current_name, current_parts)
    return traits


def normalize_size(size_text):
    mapping = {
        "tiny": "Крошечный",
        "small": "Маленький",
        "medium": "Средний",
        "large": "Большой",
        "крошечный": "Крошечный",
        "маленький": "Маленький",
        "средний": "Средний",
        "большой": "Большой",
    }
    lowered = clean_text(size_text).lower()
    return mapping.get(lowered, clean_text(size_text) or "Средний")


def parse_speed(page_text):
    match = re.search(
        r"Скорость\s*\.\s*(?:Ваша\s+базовая\s+скорость(?:\s+ходьбы)?(?:\s+составляет|\s+равна)?|Ваша\s+скорость\s+ходьбы)\s*(\d+)",
        page_text,
        re.IGNORECASE,
    )
    if match:
        return int(match.group(1))
    return 30


def parse_flying_speed(page_text):
    match = re.search(r"скорост(?:ь|ью)\s+пол[её]та\s*(?:составляет|равна)?\s*(\d+)", page_text, re.IGNORECASE)
    if match:
        return int(match.group(1))
    return 0


def parse_size(page_text):
    match = re.search(
        r"Размер\s*\.\s*[^.]{0,160}?размер\s+[—-]\s+(Маленький|Средний|Большой|Tiny|Small|Medium|Large)",
        page_text,
        re.IGNORECASE,
    )
    if match:
        return normalize_size(match.group(1))
    return "Средний"


def parse_languages_from_text(page_text, traits):
    language_text = ""
    for key, value in traits.items():
        if "язык" in key.lower():
            language_text = value
            break

    if not language_text:
        match = re.search(
            r"Языки(?:\s*[.:]|\s+)\s*(.+?)(?=\s+[А-ЯЁA-Z][А-ЯЁA-Zа-яёa-z()' -]{2,40}(?:\s*[.:]|\s{2,})|$)",
            page_text,
            re.IGNORECASE,
        )
        if match:
            language_text = match.group(1)

    if not language_text:
        return []

    working = clean_text(language_text)
    working = re.split(r"Примечание|Источник", working, maxsplit=1)[0]
    parts = []
    primary_sentence = re.split(r"(?<=[.!?])\s+", working, maxsplit=1)[0]

    if re.search(r"ещ[её]\s+(?:на\s+)?одн(?:ом|ой)?\s+язык", primary_sentence, re.IGNORECASE) or re.search(
        r"дополнительн(?:ый|ом|ого)?\s+язык|по\s+вашему\s+выбору",
        primary_sentence,
        re.IGNORECASE,
    ):
        parts.append("Дополнительный язык по выбору")

    for key, normalized in LANGUAGE_NORMALIZATION.items():
        if re.search(rf"(?<![А-Яа-яЁё]){re.escape(key)}(?![А-Яа-яЁё])", primary_sentence, re.IGNORECASE):
            parts.append(normalized)

    if not parts:
        working = re.sub(r"ещ[её]\s+одн(?:ом|ой)?\s+язык[аеуы][^.,;]*", "дополнительный язык по выбору", working, flags=re.IGNORECASE)
        working = re.sub(r"ещ[её]\s+на\s+одн(?:ом|ой)?\s+язык[аеуы][^.,;]*", "дополнительный язык по выбору", working, flags=re.IGNORECASE)

        match = re.search(r"на\s+(.+)", working, re.IGNORECASE)
        if match:
            working = match.group(1)

        working = working.replace(" и ", ", ")
        for chunk in working.split(","):
            chunk = clean_text(chunk)
            if not chunk:
                continue
            chunk = re.split(r"\s+(?:котор|если|поскольку|так\s+как|широко)", chunk, maxsplit=1)[0]
            chunk = chunk.replace("языке ", "").replace("язык ", "")
            lowered = chunk.lower().strip(" .;:")
            normalized = LANGUAGE_NORMALIZATION.get(lowered, chunk.strip(" .;:"))
            parts.append(normalized)

    unique = []
    for item in parts:
        if item and item not in unique:
            unique.append(item)
    return unique


def parse_asi(page_text):
    if re.search(r"тр[её]х\s+различных\s+характеристик|одной\s+характеристики\s+по\s+вашему\s+выбору", page_text, re.IGNORECASE):
        return {key: 0 for key in STAT_ALIASES}

    asi = {key: 0 for key in STAT_ALIASES}
    for key, aliases in STAT_ALIASES.items():
        for alias in aliases:
            pattern = rf"(?:увеличение\s+характеристик[^.]*?)?(?:значение\s+(?:вашей|вашего)?\s*)?{alias}[^.\n]{{0,90}}?на\s*(\d+)"
            matches = re.findall(pattern, page_text, flags=re.IGNORECASE)
            for value in matches:
                asi[key] = max(asi[key], int(value))
    return asi


def split_list_field(text):
    cleaned = clean_text(text)
    if not cleaned or cleaned.lower() == "нет":
        return []
    return dedupe_preserve_order(part.strip(" .") for part in cleaned.split(",") if part.strip())


def split_outside_parentheses(text):
    parts = []
    current = []
    depth = 0

    for char in text:
        if char == "(":
            depth += 1
        elif char == ")" and depth > 0:
            depth -= 1

        if char == "," and depth == 0:
            part = clean_text("".join(current)).strip(" .")
            if part:
                parts.append(part)
            current = []
            continue

        current.append(char)

    tail = clean_text("".join(current)).strip(" .")
    if tail:
        parts.append(tail)

    return parts


def split_inventory_field(text):
    cleaned = clean_text(text).rstrip(".")
    if not cleaned:
        return []

    parts = split_outside_parentheses(cleaned)
    merged = []
    continuation_pattern = re.compile(
        r"^(?:котор|в\s+котор|во\s+время|где\b|из\s+котор|на\s+котор)",
        re.IGNORECASE,
    )

    for part in parts:
        if merged and continuation_pattern.match(part):
            merged[-1] = f"{merged[-1]}, {part}"
        else:
            merged.append(part)

    return dedupe_preserve_order(merged)


def heading_anchor(tag):
    if not tag:
        return ""
    if tag.get("id"):
        return tag.get("id", "")
    anchor = tag.find(["span", "a"], id=True)
    return anchor.get("id", "") if anchor else ""


def clean_heading_title(text):
    return clean_text(text.replace("свернуть", "")).rstrip(":.")


def find_heading_by_id(root, anchor_id):
    target = root.find(id=anchor_id)
    if not target:
        return None
    if getattr(target, "name", None) in {"h2", "h3", "h4"}:
        return target
    for ancestor in target.parents:
        if getattr(ancestor, "name", None) in {"h2", "h3", "h4"}:
            return ancestor
    return None


def find_heading_by_title(root, title):
    normalized_title = clean_heading_title(title).lower()
    for heading in root.find_all(["h2", "h3", "h4"]):
        if clean_heading_title(heading.get_text(" ", strip=True)).lower() == normalized_title:
            return heading
    return None


def find_class_features_heading(root):
    return find_heading_by_id(root, "class-features") or find_heading_by_title(root, "Классовые умения")


def find_subclass_overview_heading(root):
    class_features_heading = find_class_features_heading(root)
    if not class_features_heading:
        return None

    for heading in class_features_heading.find_all_next("h2"):
        if is_noise_tag(heading):
            continue

        title = clean_heading_title(heading.get_text(" ", strip=True))
        anchor = heading_anchor(heading)
        classes = heading.get("class") or []

        if is_stop_heading(title) or anchor in {"unofficial", "comments", "gallery"}:
            break
        if anchor and "bigSectionTitle" in classes and "hide-next" not in classes and "hide-next-h2" not in classes:
            return heading

    return None


def parse_level_requirement(text):
    cleaned = clean_text(text)
    match = re.search(r"(\d+)\s*[-–—‑]?(?:й|го|м)?\s+уров", cleaned, re.IGNORECASE)
    if match:
        return int(match.group(1))
    match = re.search(r"(\d+)\s+уров", cleaned, re.IGNORECASE)
    if match:
        return int(match.group(1))
    return 0


def table_to_lines(table):
    lines = []
    for row in table.find_all("tr"):
        cells = [clean_text(cell.get_text(" ", strip=True)) for cell in row.find_all(["th", "td"])]
        cells = [cell for cell in cells if cell]
        if cells:
            lines.append(" | ".join(cells))
    return lines


def node_text_blocks(tag):
    if tag.name == "p":
        text = clean_text(tag.get_text(" ", strip=True))
        return [text] if text else []

    if tag.name == "ul":
        lines = []
        for item in tag.find_all("li", recursive=False):
            text = clean_text(item.get_text(" ", strip=True))
            if text:
                lines.append(f"- {text}")
        return lines

    if tag.name == "table":
        lines = table_to_lines(tag)
        return ["Таблица:", *lines] if lines else []

    if tag.name == "h4":
        text = clean_heading_title(tag.get_text(" ", strip=True))
        return [text] if text else []

    return []


def join_text_blocks(blocks):
    return "\n".join(clean_text(block) for block in blocks if clean_text(block))


def make_class_section(title, blocks):
    cleaned_title = clean_heading_title(title)
    cleaned_blocks = [clean_text(block) for block in blocks if clean_text(block)]
    level_text = ""
    description_blocks = []

    for block in cleaned_blocks:
        if not level_text and parse_level_requirement(block):
            level_text = block
            continue
        description_blocks.append(block)

    description = join_text_blocks(description_blocks)
    return {
        "title": cleaned_title,
        "description": description,
        "levelText": level_text,
        "levelRequirement": parse_level_requirement(level_text),
        "optional": "опциональ" in level_text.lower(),
    }


def parse_named_sections_after(start_heading, stop_anchor_ids=None, break_on_next_h2=True):
    stop_anchor_ids = stop_anchor_ids or set()
    intro_blocks = []
    sections = []
    current_title = ""
    current_blocks = []

    for tag in start_heading.find_all_next(["h2", "h3", "h4", "p", "ul", "table"]):
        if is_noise_tag(tag):
            continue
        if tag == start_heading:
            continue

        if tag.name == "h2":
            anchor = heading_anchor(tag)
            title = clean_heading_title(tag.get_text(" ", strip=True))
            if break_on_next_h2 or anchor in stop_anchor_ids or is_stop_heading(title):
                break

        if tag.name == "h3":
            title = clean_heading_title(tag.get_text(" ", strip=True))
            if not title:
                continue
            if current_title:
                sections.append(make_class_section(current_title, current_blocks))
            current_title = title
            current_blocks = []
            continue

        blocks = node_text_blocks(tag)
        if not blocks:
            continue

        if current_title:
            current_blocks.extend(blocks)
        else:
            intro_blocks.extend(blocks)

    if current_title:
        sections.append(make_class_section(current_title, current_blocks))

    sections = [section for section in sections if section["title"] and section["description"]]
    return join_text_blocks(intro_blocks), sections


def parse_class_progression(root):
    table = root.find("table", class_="class_table")
    if not table:
        return []

    progression = []
    for row in table.find_all("tr"):
        cells = [clean_text(cell.get_text(" ", strip=True)) for cell in row.find_all("td")]
        if len(cells) < 3:
            continue

        level_match = re.fullmatch(r"\d+", cells[0])
        prof_match = re.fullmatch(r"[+-]\d+", cells[1].replace(" ", ""))
        if not level_match or not prof_match:
            continue

        progression.append(
            {
                "level": int(level_match.group(0)),
                "proficiencyBonus": int(prof_match.group(0).replace("+", "")),
                "features": split_outside_parentheses(cells[2]),
            }
        )

    return progression


def parse_skill_choices(field_text):
    cleaned = clean_text(field_text)
    if not cleaned or cleaned.lower() == "нет":
        return [], 0

    choice_count = 0
    match = re.search(r"Выберите\s+([^\s:]+)", cleaned, re.IGNORECASE)
    if match:
        token = match.group(1).lower()
        choice_count = NUMBER_WORDS.get(token, 0)
        if choice_count == 0 and token.isdigit():
            choice_count = int(token)

    lowered = cleaned.lower()
    if re.search(r"\bлюб(?:ой|ые|ых)\b", lowered) and "из следующих" not in lowered:
        return ["Любой навык"], choice_count

    options_text = cleaned
    if ":" in options_text:
        options_text = options_text.split(":", 1)[1]
    options_text = re.sub(r"^Выберите\s+.+?из\s+следующих\s*:?\s*", "", options_text, flags=re.IGNORECASE)
    return split_list_field(options_text), choice_count


def parse_multiclass_info(root):
    heading = next(
        (
            tag
            for tag in root.find_all(["h3", "h4"])
            if clean_heading_title(tag.get_text(" ", strip=True)).lower().startswith("мультиклассирование")
        ),
        None,
    )
    if not heading:
        return {
            "requirement": "",
            "proficiencies": [],
            "spellcastingNote": "",
        }

    lines = []
    for tag in heading.find_all_next(["h2", "h3", "p", "ul"]):
        if is_noise_tag(tag):
            continue
        if tag == heading:
            continue
        if tag.name in {"h2", "h3"}:
            break
        lines.extend(node_text_blocks(tag))

    requirement = ""
    proficiencies = []
    spellcasting_note = ""

    for line in lines:
        lowered = line.lower()
        if lowered.startswith("минимальные значения характеристик"):
            requirement = clean_text(re.sub(r"^минимальные значения характеристик\.?\s*", "", line, flags=re.IGNORECASE))
        elif lowered.startswith("получаемые владения"):
            tail = clean_text(re.sub(r"^получаемые владения\.?\s*", "", line, flags=re.IGNORECASE))
            if ":" in tail:
                tail = tail.split(":", 1)[1]
            proficiencies = split_list_field(tail)
        elif lowered.startswith("ячейки заклинаний"):
            spellcasting_note = clean_text(re.sub(r"^ячейки заклинаний\.?\s*", "", line, flags=re.IGNORECASE)).lstrip(" .:-")

    return {
        "requirement": requirement,
        "proficiencies": proficiencies,
        "spellcastingNote": spellcasting_note,
    }


def parse_starting_equipment(root):
    equipment_heading = next(
        (
            tag
            for tag in root.find_all("h4")
            if clean_heading_title(tag.get_text(" ", strip=True)).lower() == "снаряжение"
        ),
        None,
    )
    if not equipment_heading:
        return []

    equipment = []
    for tag in equipment_heading.find_all_next(["h3", "h4", "p", "ul"]):
        if is_noise_tag(tag):
            continue
        if tag == equipment_heading:
            continue
        if tag.name in {"h3", "h4"}:
            break
        if tag.name == "ul":
            for item in tag.find_all("li", recursive=False):
                text = clean_text(item.get_text(" ", strip=True))
                if text:
                    equipment.append(text)
    return equipment


def parse_class_feature_sections(root):
    start_heading = find_class_features_heading(root)
    if not start_heading:
        return []
    _, sections = parse_named_sections_after(start_heading, break_on_next_h2=True)
    return sections


def parse_official_subclasses(root):
    start_heading = find_subclass_overview_heading(root)
    if not start_heading:
        return []

    subclasses = []
    for heading in start_heading.find_all_next("h2"):
        if is_noise_tag(heading):
            continue
        anchor = heading_anchor(heading)
        title = clean_heading_title(heading.get_text(" ", strip=True))
        classes = heading.get("class") or []

        if heading == start_heading:
            continue
        if anchor == "unofficial" or anchor.startswith("ua.") or anchor.startswith("hb."):
            break
        if "unearthed arcana" in title.lower() or "homebrew" in title.lower():
            break
        if is_stop_heading(title):
            break
        if anchor and "bigSectionTitle" in classes and "hide-next" not in classes and "hide-next-h2" not in classes:
            break
        if "bigSectionTitle" not in classes or "hide-next" not in classes or "hide-next-h2" not in classes:
            continue

        description, sections = parse_named_sections_after(heading, break_on_next_h2=True)
        subclasses.append(
            {
                "name": title,
                "description": description,
                "sections": sections,
            }
        )

    return subclasses


def extract_root_paragraphs(root):
    paragraphs = []
    for tag in root.find_all("p"):
        if is_noise_tag(tag):
            continue
        text = clean_text(tag.get_text(" ", strip=True))
        if text:
            paragraphs.append(text)
    return paragraphs


def extract_heading_sections(root):
    sections = {}
    current_title = ""
    current_parts = []

    def commit_section():
        if current_title and current_parts:
            sections[current_title] = clean_text(" ".join(current_parts))

    for tag in root.find_all(["h3", "h4", "p"]):
        if is_noise_tag(tag):
            continue

        text = clean_text(tag.get_text(" ", strip=True))
        if not text:
            continue

        if tag.name in {"h3", "h4"}:
            if is_stop_heading(text):
                break
            commit_section()
            current_title = text.rstrip(":.")
            current_parts = []
            continue

        if current_title:
            current_parts.append(text)

    commit_section()
    return sections


def parse_background_field(paragraphs, prefix):
    lowered_prefix = prefix.lower()
    for paragraph in paragraphs:
        if paragraph.lower().startswith(lowered_prefix):
            return clean_text(paragraph.split(":", 1)[1] if ":" in paragraph else paragraph[len(prefix):])
    return ""


def extract_background_feature(sections):
    for title, value in sections.items():
        match = re.match(r"(?:АЛЬТЕРНАТИВНОЕ\s+)?УМЕНИЕ:\s*(.+)", title, re.IGNORECASE)
        if match:
            return clean_text(match.group(1)), value
    return "", ""


def enrich_background_variants(backgrounds):
    by_name = {background["name"]: background for background in backgrounds}

    for background in backgrounds:
        if not background["name"].endswith(" Врат Балдура"):
            continue

        base_name = background["name"][: -len(" Врат Балдура")]
        base_background = by_name.get(base_name)
        if not base_background:
            continue

        if not background.get("featureName"):
            background["featureName"] = base_background.get("featureName", "")
        if not background.get("featureDescription"):
            background["featureDescription"] = base_background.get("featureDescription", "")
        if not background.get("equipment"):
            background["equipment"] = list(base_background.get("equipment", []))
        if not background.get("skillProficiencies"):
            background["skillProficiencies"] = list(base_background.get("skillProficiencies", []))
        if not background.get("toolProficiencies"):
            background["toolProficiencies"] = list(base_background.get("toolProficiencies", []))
        if not background.get("languages"):
            background["languages"] = list(base_background.get("languages", []))

        if background.get("featureName") and background.get("featureDescription"):
            feature_key = f"УМЕНИЕ: {background['featureName']}"
            background.setdefault("traits", {})
            background["traits"].setdefault(feature_key, background["featureDescription"])


def parse_backgrounds():
    links = collect_links(BACKGROUNDS_URL, r"/backgrounds/\d+-[^/]+/")
    backgrounds = []

    for url in links:
        print(f"Parsing background: {url}")
        soup = fetch_soup(url)
        root = extract_content_root(soup)
        name = extract_name(soup)
        paragraphs = extract_root_paragraphs(root)
        sections = extract_heading_sections(root)
        feature_name, feature_description = extract_background_feature(sections)

        description = ""
        for paragraph in paragraphs:
            if not re.match(r"^(Владение навыками|Владение инструментами|Языки|Снаряжение):", paragraph, re.IGNORECASE):
                description = paragraph
                break

        background = {
            "slug": get_slug(url),
            "name": name,
            "source": extract_source(soup),
            "description": description,
            "skillProficiencies": split_list_field(parse_background_field(paragraphs, "Владение навыками")),
            "toolProficiencies": split_list_field(parse_background_field(paragraphs, "Владение инструментами")),
            "languages": split_list_field(parse_background_field(paragraphs, "Языки")),
            "equipment": split_inventory_field(parse_background_field(paragraphs, "Снаряжение")),
            "featureName": feature_name,
            "featureDescription": feature_description,
            "traits": sections,
        }

        backgrounds.append(background)
        time.sleep(0.1)

    enrich_background_variants(backgrounds)

    output_path = REPO_ROOT / "backgrounds_dndsu.json"
    output_path.write_text(json.dumps(backgrounds, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Saved {len(backgrounds)} backgrounds to {output_path}")


def parse_feat_prerequisite(root):
    for item in root.find_all("li"):
        if is_noise_tag(item):
            continue
        text = clean_text(item.get_text(" ", strip=True))
        if text.lower().startswith("требование:"):
            return clean_text(text.split(":", 1)[1])
    return ""


def parse_feat_benefits(root, description):
    benefits = []
    for item in root.find_all("li"):
        if is_noise_tag(item):
            continue
        text = clean_text(item.get_text(" ", strip=True))
        if not text or text.lower().startswith("требование:"):
            continue
        if description and text.startswith(description) and len(text) > len(description) + 40:
            continue
        benefits.append(text)

    if benefits:
        return dedupe_preserve_order(benefits)

    paragraphs = extract_root_paragraphs(root)
    if len(paragraphs) > 1:
        return dedupe_preserve_order(paragraphs[1:])

    if description:
        return [description]

    return []


def parse_feats():
    links = collect_links(FEATS_URL, r"/feats/\d+-[^/]+/")
    feats = []

    for url in links:
        print(f"Parsing feat: {url}")
        soup = fetch_soup(url)
        root = extract_content_root(soup)
        name = extract_name(soup)
        paragraphs = extract_root_paragraphs(root)
        description = paragraphs[0] if paragraphs else ""

        feat = {
            "slug": get_slug(url),
            "name": name,
            "source": extract_source(soup),
            "description": description,
            "prerequisite": parse_feat_prerequisite(root),
            "benefits": parse_feat_benefits(root, description),
        }

        feats.append(feat)
        time.sleep(0.1)

    output_path = REPO_ROOT / "feats_dndsu.json"
    output_path.write_text(json.dumps(feats, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Saved {len(feats)} feats to {output_path}")


def extract_section_paragraphs(soup, heading_text):
    normalized_heading = heading_text.lower()
    heading = next(
        (
            tag
            for tag in soup.find_all(["h3", "h4"])
            if clean_text(tag.get_text(" ", strip=True)).lower() == normalized_heading
        ),
        None,
    )
    if not heading:
        return []

    paragraphs = []
    for sibling in heading.next_siblings:
        if getattr(sibling, "name", None) in ("h2", "h3", "h4"):
            break
        if getattr(sibling, "name", None) is None:
            text = clean_text(str(sibling))
            if text:
                paragraphs.append(text)
            continue
        if sibling.name in ("h2", "h3", "h4"):
            break
        if sibling.name != "p":
            continue
        text = clean_text(sibling.get_text(" ", strip=True))
        if text:
            paragraphs.append(text)

    return paragraphs


def parse_sidekick_class_fields(soup):
    paragraphs = extract_section_paragraphs(soup, "Бонусные владения")
    if not paragraphs:
        return {"savingThrows": "", "armor": "", "weapons": ""}

    result = {"savingThrows": "", "armor": "", "weapons": ""}
    for paragraph in paragraphs:
        lowered = paragraph.lower()

        if not result["savingThrows"] and "спасбросков" in lowered and ":" in paragraph:
            result["savingThrows"] = clean_text(paragraph.split(":", 1)[1].rstrip("."))

        if not result["armor"]:
            if "владение всеми типами доспехов" in lowered:
                result["armor"] = "Все доспехи, щиты"
            elif "владение лёгкими доспехами" in lowered or "владение легкими доспехами" in lowered:
                result["armor"] = "Лёгкие доспехи"

        if not result["weapons"]:
            if "всеми видами простого и воинского оружия" in lowered:
                result["weapons"] = "Простое оружие, воинское оружие"
            elif "всеми видами простого оружия" in lowered or "всеми простыми видами оружия" in lowered:
                result["weapons"] = "Простое оружие"

    return result


def parse_class_field(page_text, field_name, next_field):
    match = re.search(
        rf"{field_name}\s*:\s*(.+?)(?={next_field}\s*:|$)",
        page_text,
        re.IGNORECASE,
    )
    return clean_text(match.group(1)) if match else ""


def parse_class_labeled_field(root, label):
    normalized_label = clean_text(label).lower()

    for paragraph in root.find_all("p"):
        if is_noise_tag(paragraph):
            continue

        strong = paragraph.find("strong")
        if not strong:
            continue

        strong_text = clean_text(strong.get_text(" ", strip=True)).rstrip(":.")
        if strong_text.lower() != normalized_label:
            continue

        full_text = clean_text(paragraph.get_text(" ", strip=True))
        pattern = rf"^{re.escape(strong_text)}\s*:?\s*"
        return clean_text(re.sub(pattern, "", full_text, count=1, flags=re.IGNORECASE))

    return ""


def parse_class_hit_die(page_text):
    match = re.search(r"Кость\s+Хитов\s*:\s*1к(\d+)", page_text, re.IGNORECASE)
    if match:
        return int(match.group(1))
    return 8


def parse_classes():
    links = collect_links(CLASSES_URL, r"/class/\d+-[^/]+/")
    classes = []

    for url in links:
        print(f"Parsing class: {url}")
        soup = fetch_soup(url)
        root = extract_content_root(soup)
        name = extract_name(soup)
        page_text = soup.get_text(" ", strip=True)
        sidekick_fields = parse_sidekick_class_fields(soup)
        saves_field = parse_class_labeled_field(root, "Спасброски") or parse_class_field(page_text, "Спасброски", "Навыки") or sidekick_fields["savingThrows"]
        armor_field = parse_class_labeled_field(root, "Доспехи") or parse_class_field(page_text, "Доспехи", "Оружие") or sidekick_fields["armor"]
        weapons_field = parse_class_labeled_field(root, "Оружие") or parse_class_field(page_text, "Оружие", "Инструменты") or sidekick_fields["weapons"]
        tools_field = parse_class_labeled_field(root, "Инструменты") or parse_class_field(page_text, "Инструменты", "Спасброски")
        skills_field = parse_class_labeled_field(root, "Навыки") or parse_class_field(page_text, "Навыки", "Снаряжение")
        skill_choices, skill_choice_count = parse_skill_choices(skills_field)
        multiclass_info = parse_multiclass_info(root)

        cls = {
            "slug": get_slug(url),
            "name": name,
            "source": extract_source(soup),
            "description": extract_description(soup, name) or CLASS_DESCRIPTIONS.get(name, ""),
            "hitDie": parse_class_hit_die(page_text),
            "primaryAbility": CLASS_PRIMARY_ABILITIES.get(name, ""),
            "savingThrowProficiencies": split_list_field(saves_field),
            "armorProficiencies": split_list_field(armor_field),
            "weaponProficiencies": split_list_field(weapons_field),
            "toolProficiencies": split_list_field(tools_field),
            "skillChoices": skill_choices,
            "skillChoiceCount": skill_choice_count,
            "startingEquipment": parse_starting_equipment(root),
            "multiclassRequirement": multiclass_info["requirement"],
            "multiclassProficiencies": multiclass_info["proficiencies"],
            "multiclassSpellcastingNote": multiclass_info["spellcastingNote"],
            "progression": parse_class_progression(root),
            "featureSections": parse_class_feature_sections(root),
            "subclasses": parse_official_subclasses(root),
        }

        classes.append(cls)
        time.sleep(0.1)

    output_path = REPO_ROOT / "classes_dndsu.json"
    output_path.write_text(json.dumps(classes, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Saved {len(classes)} classes to {output_path}")


def parse_races():
    links = collect_links(RACES_URL, r"/(?:race|multiverse/race)/\d+-[^/]+/")
    races = []

    for url in links:
        print(f"Parsing race: {url}")
        soup = fetch_soup(url)
        name = extract_name(soup)
        traits = extract_traits(soup)
        page_text = soup.get_text(" ", strip=True)

        race = {
            "slug": get_slug(url),
            "name": name,
            "source": extract_source(soup),
            "description": extract_description(soup, name),
            "speed": parse_speed(page_text),
            "flyingSpeed": parse_flying_speed(page_text),
            "size": parse_size(page_text),
            "asi": parse_asi(page_text),
            "traits": traits,
            "languages": parse_languages_from_text(page_text, traits),
        }

        races.append(race)
        time.sleep(0.1)

    races = normalize_race_entries(races)
    output_path = REPO_ROOT / "races_dndsu.json"
    output_path.write_text(json.dumps(races, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Saved {len(races)} races to {output_path}")


if __name__ == "__main__":
    parse_races()
    parse_classes()
    parse_backgrounds()
    parse_feats()