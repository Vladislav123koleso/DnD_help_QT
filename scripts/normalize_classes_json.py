from __future__ import annotations

import json
import re
from collections import OrderedDict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CLASSES_PATH = ROOT / "classes_dndsu.json"


CLASS_FIXES = {
    "Бард": {
        "hitDie": 8,
        "primaryAbility": "Харизма",
        "savingThrowProficiencies": ["Ловкость", "Харизма"],
        "armorProficiencies": ["Лёгкие доспехи"],
        "weaponProficiencies": ["Простое оружие", "Ручные арбалеты", "Длинные мечи", "Рапиры", "Короткие мечи"],
        "toolProficiencies": ["Три музыкальных инструмента на ваш выбор"],
        "skillChoices": ["Любой навык"],
        "skillChoiceCount": 3,
        "startingEquipment": [
            "а) рапира, б) длинный меч или в) любое простое оружие",
            "а) набор дипломата или б) набор артиста",
            "а) лютня или б) любой другой музыкальный инструмент",
            "Кожаный доспех, кинжал",
        ],
        "multiclassRequirement": "Харизма 13 или выше.",
        "multiclassProficiencies": ["Лёгкие доспехи", "Один навык на ваш выбор", "Один музыкальный инструмент на ваш выбор"],
    },
    "Варвар": {
        "hitDie": 12,
        "primaryAbility": "Сила",
        "savingThrowProficiencies": ["Сила", "Телосложение"],
        "armorProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты"],
        "weaponProficiencies": ["Простое оружие", "Воинское оружие"],
        "toolProficiencies": [],
        "skillChoices": ["Атлетика", "Восприятие", "Выживание", "Запугивание", "Природа", "Уход за животными"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "а) секира или б) любое воинское рукопашное оружие",
            "а) два ручных топора или б) любое простое оружие",
            "Набор путешественника, четыре метательных копья",
        ],
        "multiclassRequirement": "Сила 13 или выше.",
        "multiclassProficiencies": ["Щиты", "Простое оружие", "Воинское оружие"],
    },
    "Воин": {
        "hitDie": 10,
        "primaryAbility": "Сила или Ловкость",
        "savingThrowProficiencies": ["Сила", "Телосложение"],
        "armorProficiencies": ["Все доспехи", "Щиты"],
        "weaponProficiencies": ["Простое оружие", "Воинское оружие"],
        "toolProficiencies": [],
        "skillChoices": ["Акробатика", "Атлетика", "Восприятие", "Выживание", "Запугивание", "История", "Проницательность", "Уход за животными"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "а) кольчуга или б) кожаный доспех, длинный лук и 20 стрел",
            "а) воинское оружие и щит или б) два воинских оружия",
            "а) лёгкий арбалет и 20 болтов или б) два ручных топора",
            "а) набор исследователя подземелий или б) набор путешественника",
        ],
        "multiclassRequirement": "Сила 13 или Ловкость 13 или выше.",
        "multiclassProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты", "Простое оружие", "Воинское оружие"],
    },
    "Волшебник": {
        "hitDie": 6,
        "primaryAbility": "Интеллект",
        "savingThrowProficiencies": ["Интеллект", "Мудрость"],
        "armorProficiencies": [],
        "weaponProficiencies": ["Кинжалы", "Дротики", "Пращи", "Боевые посохи", "Лёгкие арбалеты"],
        "toolProficiencies": [],
        "skillChoices": ["История", "Магия", "Медицина", "Проницательность", "Расследование", "Религия"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "а) боевой посох или б) кинжал",
            "а) мешочек с компонентами или б) магическая фокусировка",
            "а) набор учёного или б) набор путешественника",
            "Книга заклинаний",
        ],
        "multiclassRequirement": "Интеллект 13 или выше.",
        "multiclassProficiencies": [],
    },
    "Друид": {
        "hitDie": 8,
        "primaryAbility": "Мудрость",
        "savingThrowProficiencies": ["Интеллект", "Мудрость"],
        "armorProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты"],
        "weaponProficiencies": ["Дубинки", "Кинжалы", "Дротики", "Метательные копья", "Булавы", "Боевые посохи", "Скимитары", "Серп", "Пращи", "Копья"],
        "toolProficiencies": ["Набор травника"],
        "skillChoices": ["Восприятие", "Выживание", "Магия", "Медицина", "Природа", "Проницательность", "Религия", "Уход за животными"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "а) деревянный щит или б) любое простое оружие",
            "а) скимитар или б) любое простое рукопашное оружие",
            "Кожаный доспех, набор путешественника, фокусировка друидов",
        ],
        "multiclassRequirement": "Мудрость 13 или выше.",
        "multiclassProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты"],
    },
    "Жрец": {
        "hitDie": 8,
        "primaryAbility": "Мудрость",
        "savingThrowProficiencies": ["Мудрость", "Харизма"],
        "armorProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты"],
        "weaponProficiencies": ["Простое оружие"],
        "toolProficiencies": [],
        "skillChoices": ["История", "Медицина", "Проницательность", "Религия", "Убеждение"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "а) булава или б) боевой молот, если владеете им",
            "а) чешуйчатый доспех, б) кожаный доспех или в) кольчуга, если владеете ей",
            "а) лёгкий арбалет и 20 болтов или б) любое простое оружие",
            "а) набор священника или б) набор путешественника",
            "Щит, священный символ",
        ],
        "multiclassRequirement": "Мудрость 13 или выше.",
        "multiclassProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты"],
    },
    "Изобретатель": {
        "hitDie": 8,
        "primaryAbility": "Интеллект",
        "savingThrowProficiencies": ["Телосложение", "Интеллект"],
        "armorProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты"],
        "weaponProficiencies": ["Простое оружие"],
        "toolProficiencies": ["Воровские инструменты", "Инструменты ремонтника", "Один вид ремесленных инструментов на ваш выбор"],
        "skillChoices": ["Восприятие", "История", "Ловкость рук", "Магия", "Медицина", "Природа", "Расследование"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "Любые два простых оружия",
            "Лёгкий арбалет и 20 болтов",
            "а) проклёпанный кожаный доспех или б) чешуйчатый доспех",
            "Воровские инструменты, набор исследователя подземелий",
        ],
        "multiclassRequirement": "Интеллект 13 или выше.",
        "multiclassProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты", "Воровские инструменты", "Инструменты ремонтника"],
    },
    "Колдун": {
        "hitDie": 8,
        "primaryAbility": "Харизма",
        "savingThrowProficiencies": ["Мудрость", "Харизма"],
        "armorProficiencies": ["Лёгкие доспехи"],
        "weaponProficiencies": ["Простое оружие"],
        "toolProficiencies": [],
        "skillChoices": ["Запугивание", "История", "Магия", "Обман", "Природа", "Расследование", "Религия"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "а) лёгкий арбалет и 20 болтов или б) любое простое оружие",
            "а) мешочек с компонентами или б) магическая фокусировка",
            "а) набор учёного или б) набор исследователя подземелий",
            "Кожаный доспех, любое простое оружие, два кинжала",
        ],
        "multiclassRequirement": "Харизма 13 или выше.",
        "multiclassProficiencies": ["Лёгкие доспехи", "Простое оружие"],
    },
    "Монах": {
        "hitDie": 8,
        "primaryAbility": "Ловкость и Мудрость",
        "savingThrowProficiencies": ["Сила", "Ловкость"],
        "armorProficiencies": [],
        "weaponProficiencies": ["Простое оружие", "Короткие мечи"],
        "toolProficiencies": ["Один вид ремесленных инструментов или один музыкальный инструмент на ваш выбор"],
        "skillChoices": ["Акробатика", "Атлетика", "История", "Проницательность", "Религия", "Скрытность"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "а) короткий меч или б) любое простое оружие",
            "а) набор исследователя подземелий или б) набор путешественника",
            "10 дротиков",
        ],
        "multiclassRequirement": "Ловкость 13 и Мудрость 13 или выше.",
        "multiclassProficiencies": ["Простое оружие", "Короткие мечи"],
    },
    "Паладин": {
        "hitDie": 10,
        "primaryAbility": "Сила и Харизма",
        "savingThrowProficiencies": ["Мудрость", "Харизма"],
        "armorProficiencies": ["Все доспехи", "Щиты"],
        "weaponProficiencies": ["Простое оружие", "Воинское оружие"],
        "toolProficiencies": [],
        "skillChoices": ["Атлетика", "Запугивание", "Медицина", "Проницательность", "Религия", "Убеждение"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "а) воинское оружие и щит или б) два воинских оружия",
            "а) пять метательных копий или б) любое простое рукопашное оружие",
            "а) набор священника или б) набор путешественника",
            "Кольчуга, священный символ",
        ],
        "multiclassRequirement": "Сила 13 и Харизма 13 или выше.",
        "multiclassProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты", "Простое оружие", "Воинское оружие"],
    },
    "Плут": {
        "hitDie": 8,
        "primaryAbility": "Ловкость",
        "savingThrowProficiencies": ["Ловкость", "Интеллект"],
        "armorProficiencies": ["Лёгкие доспехи"],
        "weaponProficiencies": ["Простое оружие", "Ручные арбалеты", "Длинные мечи", "Рапиры", "Короткие мечи"],
        "toolProficiencies": ["Воровские инструменты"],
        "skillChoices": ["Акробатика", "Атлетика", "Восприятие", "Выступление", "Запугивание", "Ловкость рук", "Обман", "Проницательность", "Расследование", "Скрытность", "Убеждение"],
        "skillChoiceCount": 4,
        "startingEquipment": [
            "а) рапира или б) короткий меч",
            "а) короткий лук и колчан с 20 стрелами или б) короткий меч",
            "а) набор взломщика, б) набор исследователя подземелий или в) набор путешественника",
            "Кожаный доспех, два кинжала, воровские инструменты",
        ],
        "multiclassRequirement": "Ловкость 13 или выше.",
        "multiclassProficiencies": ["Лёгкие доспехи", "Один навык из списка навыков плута", "Воровские инструменты"],
    },
    "Следопыт": {
        "hitDie": 10,
        "primaryAbility": "Ловкость и Мудрость",
        "savingThrowProficiencies": ["Сила", "Ловкость"],
        "armorProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты"],
        "weaponProficiencies": ["Простое оружие", "Воинское оружие"],
        "toolProficiencies": [],
        "skillChoices": ["Атлетика", "Восприятие", "Выживание", "Природа", "Проницательность", "Расследование", "Скрытность", "Уход за животными"],
        "skillChoiceCount": 3,
        "startingEquipment": [
            "а) чешуйчатый доспех или б) кожаный доспех",
            "а) два коротких меча или б) два простых рукопашных оружия",
            "а) набор исследователя подземелий или б) набор путешественника",
            "Длинный лук и колчан с 20 стрелами",
        ],
        "multiclassRequirement": "Ловкость 13 и Мудрость 13 или выше.",
        "multiclassProficiencies": ["Лёгкие доспехи", "Средние доспехи", "Щиты", "Простое оружие", "Воинское оружие", "Один навык из списка навыков следопыта"],
    },
    "Чародей": {
        "hitDie": 6,
        "primaryAbility": "Харизма",
        "savingThrowProficiencies": ["Телосложение", "Харизма"],
        "armorProficiencies": [],
        "weaponProficiencies": ["Кинжалы", "Дротики", "Пращи", "Боевые посохи", "Лёгкие арбалеты"],
        "toolProficiencies": [],
        "skillChoices": ["Запугивание", "Магия", "Обман", "Проницательность", "Религия", "Убеждение"],
        "skillChoiceCount": 2,
        "startingEquipment": [
            "а) лёгкий арбалет и 20 болтов или б) любое простое оружие",
            "а) мешочек с компонентами или б) магическая фокусировка",
            "а) набор исследователя подземелий или б) набор путешественника",
            "Два кинжала",
        ],
        "multiclassRequirement": "Харизма 13 или выше.",
        "multiclassProficiencies": [],
    },
    "Боец": {
        "hitDie": 8,
        "primaryAbility": "Сила или Ловкость",
        "savingThrowProficiencies": ["Сила", "Телосложение"],
        "armorProficiencies": ["Все доспехи", "Щиты"],
        "weaponProficiencies": ["Простое оружие", "Воинское оружие"],
        "toolProficiencies": [],
        "skillChoices": ["Любой навык"],
        "skillChoiceCount": 2,
        "startingEquipment": [],
        "multiclassRequirement": "",
        "multiclassProficiencies": [],
    },
    "Заклинатель": {
        "hitDie": 8,
        "primaryAbility": "Зависит от роли",
        "savingThrowProficiencies": ["Мудрость"],
        "armorProficiencies": ["Лёгкие доспехи"],
        "weaponProficiencies": ["Простое оружие"],
        "toolProficiencies": [],
        "skillChoices": ["Любой навык"],
        "skillChoiceCount": 2,
        "startingEquipment": [],
        "multiclassRequirement": "",
        "multiclassProficiencies": [],
    },
    "Эксперт": {
        "hitDie": 8,
        "primaryAbility": "Зависит от роли",
        "savingThrowProficiencies": ["Ловкость"],
        "armorProficiencies": ["Лёгкие доспехи"],
        "weaponProficiencies": ["Простое оружие"],
        "toolProficiencies": ["Два инструмента на ваш выбор"],
        "skillChoices": ["Любой навык"],
        "skillChoiceCount": 5,
        "startingEquipment": [],
        "multiclassRequirement": "",
        "multiclassProficiencies": [],
    },
}


PRIMARY_ORDER = [
    "Бард",
    "Варвар",
    "Воин",
    "Волшебник",
    "Друид",
    "Жрец",
    "Изобретатель",
    "Колдун",
    "Монах",
    "Паладин",
    "Плут",
    "Следопыт",
    "Чародей",
    "Боец",
    "Заклинатель",
    "Эксперт",
]


def clean_text(value: str) -> str:
    return re.sub(r"[ \t]+", " ", (value or "").replace("\xa0", " ")).strip()


def unique_list(values):
    result = []
    seen = set()
    for value in values or []:
        cleaned = clean_text(str(value)).strip(" .")
        if not cleaned or cleaned == "-":
            continue
        key = cleaned.lower()
        if key not in seen:
            result.append(cleaned)
            seen.add(key)
    return result


def clean_progression(progression):
    cleaned = []
    for entry in progression or []:
        level = int(entry.get("level", 0) or 0)
        if level <= 0:
            continue
        proficiency = int(entry.get("proficiencyBonus", 0) or 0)
        if proficiency <= 0:
            proficiency = 2 + max(0, level - 1) // 4
        cleaned.append(
            OrderedDict(
                [
                    ("level", level),
                    ("proficiencyBonus", proficiency),
                    ("features", unique_list(entry.get("features", []))),
                ]
            )
        )

    by_level = {entry["level"]: entry for entry in cleaned}
    complete = []
    for level in range(1, 21):
        complete.append(
            by_level.get(
                level,
                OrderedDict(
                    [
                        ("level", level),
                        ("proficiencyBonus", 2 + max(0, level - 1) // 4),
                        ("features", []),
                    ]
                ),
            )
        )
    return complete


def infer_level_requirement(text: str) -> int:
    match = re.search(r"(\d+)\s*[-–—‑]?(?:й|го|м)?\s+уров", text or "", re.IGNORECASE)
    return int(match.group(1)) if match else 0


def clean_sections(sections):
    result = []
    seen = set()
    ignored_titles = {"Источник", "Галерея", "Комментарии", "Показать email", "Распечатать"}
    for section in sections or []:
        title = clean_text(section.get("title", "")).strip(":")
        description = clean_text(section.get("description", ""))
        if not title or not description or title in ignored_titles:
            continue
        key = title.lower()
        if key in seen:
            continue
        seen.add(key)
        level_text = clean_text(section.get("levelText", ""))
        level = int(section.get("levelRequirement", 0) or 0) or infer_level_requirement(level_text) or infer_level_requirement(description)
        result.append(
            OrderedDict(
                [
                    ("title", title),
                    ("description", description),
                    ("levelText", level_text),
                    ("levelRequirement", level),
                    ("optional", bool(section.get("optional", False)) or "опциональ" in level_text.lower()),
                ]
            )
        )
    return result


def clean_subclasses(subclasses):
    result = []
    seen = set()
    for subclass in subclasses or []:
        name = clean_text(subclass.get("name", ""))
        if not name:
            continue
        key = name.lower()
        if key in seen:
            continue
        seen.add(key)
        result.append(
            OrderedDict(
                [
                    ("name", name),
                    ("description", clean_text(subclass.get("description", ""))),
                    ("sections", clean_sections(subclass.get("sections", []))),
                ]
            )
        )
    return result


def normalize_class(cls):
    name = cls.get("name", "")
    fixed = CLASS_FIXES.get(name, {})
    normalized = OrderedDict()
    for key in ("slug", "name", "source", "description"):
        normalized[key] = clean_text(cls.get(key, ""))

    for key in (
        "hitDie",
        "primaryAbility",
        "savingThrowProficiencies",
        "armorProficiencies",
        "weaponProficiencies",
        "toolProficiencies",
        "skillChoices",
        "skillChoiceCount",
        "startingEquipment",
        "multiclassRequirement",
        "multiclassProficiencies",
    ):
        normalized[key] = fixed.get(key, cls.get(key, [] if key.endswith("Proficiencies") or key in {"skillChoices", "startingEquipment"} else ""))

    normalized["savingThrowProficiencies"] = unique_list(normalized["savingThrowProficiencies"])
    normalized["armorProficiencies"] = unique_list(normalized["armorProficiencies"])
    normalized["weaponProficiencies"] = unique_list(normalized["weaponProficiencies"])
    normalized["toolProficiencies"] = unique_list(normalized["toolProficiencies"])
    normalized["skillChoices"] = unique_list(normalized["skillChoices"])
    normalized["skillChoiceCount"] = int(normalized.get("skillChoiceCount") or 0)
    normalized["startingEquipment"] = unique_list(normalized["startingEquipment"])
    normalized["multiclassProficiencies"] = unique_list(normalized["multiclassProficiencies"])
    normalized["multiclassSpellcastingNote"] = clean_text(cls.get("multiclassSpellcastingNote", ""))
    normalized["progression"] = clean_progression(cls.get("progression", []))
    normalized["featureSections"] = clean_sections(cls.get("featureSections", []))
    normalized["subclasses"] = clean_subclasses(cls.get("subclasses", []))
    return normalized


def main():
    classes = json.loads(CLASSES_PATH.read_text(encoding="utf-8"), object_pairs_hook=OrderedDict)
    by_name = {cls.get("name", ""): cls for cls in classes}
    ordered_names = [name for name in PRIMARY_ORDER if name in by_name]
    ordered_names.extend(name for name in by_name if name not in ordered_names)
    normalized = [normalize_class(by_name[name]) for name in ordered_names]
    CLASSES_PATH.write_text(json.dumps(normalized, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Normalized {len(normalized)} classes")


if __name__ == "__main__":
    main()
