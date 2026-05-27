#include "npc.h"

#include "character.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

namespace {

QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            array.append(trimmed);
        }
    }
    return array;
}

QStringList jsonToStringList(const QJsonValue &value)
{
    QStringList result;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        const QString trimmed = entry.toString().trimmed();
        if (!trimmed.isEmpty()) {
            result << trimmed;
        }
    }
    return result;
}

QJsonObject stringMapToJson(const QMap<QString, QString> &map)
{
    QJsonObject object;
    for (auto it = map.begin(); it != map.end(); ++it) {
        const QString key = it.key().trimmed();
        const QString text = it.value().trimmed();
        if (!key.isEmpty() && !text.isEmpty()) {
            object.insert(key, text);
        }
    }
    return object;
}

QMap<QString, QString> jsonToStringMap(const QJsonValue &value)
{
    QMap<QString, QString> map;
    const QJsonObject object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
        const QString text = it.value().toString().trimmed();
        if (!text.isEmpty()) {
            map.insert(it.key(), text);
        }
    }
    return map;
}

QJsonObject spellToJson(const Spell &spell)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), spell.name);
    object.insert(QStringLiteral("level"), spell.level);
    object.insert(QStringLiteral("school"), spell.school);
    object.insert(QStringLiteral("description"), spell.description);
    object.insert(QStringLiteral("classes"), spell.classes);
    object.insert(QStringLiteral("source"), spell.source);
    return object;
}

Spell jsonToSpell(const QJsonValue &value)
{
    const QJsonObject object = value.toObject();
    Spell spell;
    spell.name = object.value(QStringLiteral("name")).toString();
    spell.level = object.value(QStringLiteral("level")).toInt();
    spell.school = object.value(QStringLiteral("school")).toString();
    spell.description = object.value(QStringLiteral("description")).toString();
    spell.classes = object.value(QStringLiteral("classes")).toString();
    spell.source = object.value(QStringLiteral("source")).toString();
    return spell;
}

QJsonArray spellListToJson(const QList<Spell> &spells)
{
    QJsonArray array;
    for (const Spell &spell : spells) {
        if (!spell.name.trimmed().isEmpty()) {
            array.append(spellToJson(spell));
        }
    }
    return array;
}

QList<Spell> jsonToSpellList(const QJsonValue &value)
{
    QList<Spell> spells;
    for (const QJsonValue &entry : value.toArray()) {
        spells.append(jsonToSpell(entry));
    }
    return spells;
}

QJsonObject itemToJson(const Item &item)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), item.name);
    object.insert(QStringLiteral("type"), item.type);
    object.insert(QStringLiteral("quantity"), item.quantity);
    object.insert(QStringLiteral("description"), item.description);
    return object;
}

Item jsonToItem(const QJsonValue &value)
{
    const QJsonObject object = value.toObject();
    Item item;
    item.name = object.value(QStringLiteral("name")).toString();
    item.type = object.value(QStringLiteral("type")).toString();
    item.quantity = qMax(1, object.value(QStringLiteral("quantity")).toInt(1));
    item.description = object.value(QStringLiteral("description")).toString();
    return item;
}

QJsonArray itemListToJson(const QList<Item> &items)
{
    QJsonArray array;
    for (const Item &item : items) {
        if (!item.name.trimmed().isEmpty()) {
            array.append(itemToJson(item));
        }
    }
    return array;
}

QList<Item> jsonToItemList(const QJsonValue &value)
{
    QList<Item> items;
    for (const QJsonValue &entry : value.toArray()) {
        items.append(jsonToItem(entry));
    }
    return items;
}

} // namespace

NpcFolder NpcFolder::fromJson(const QJsonObject &object)
{
    NpcFolder folder;
    folder.id = object.value(QStringLiteral("id")).toString();
    folder.name = object.value(QStringLiteral("name")).toString();
    folder.expanded = object.value(QStringLiteral("expanded")).toBool(true);
    folder.sortOrder = object.value(QStringLiteral("sortOrder")).toInt();
    return folder;
}

QJsonObject NpcFolder::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("expanded"), expanded);
    object.insert(QStringLiteral("sortOrder"), sortOrder);
    return object;
}

NpcEntry NpcEntry::createNew(const QString &listName)
{
    NpcEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.listName = listName.trimmed();
    entry.name = entry.listName;
    return entry;
}

NpcEntry NpcEntry::fromJson(const QJsonObject &object)
{
    NpcEntry entry;
    entry.id = object.value(QStringLiteral("id")).toString();
    entry.listName = object.value(QStringLiteral("listName")).toString();
    entry.folderId = object.value(QStringLiteral("folderId")).toString();
    entry.sortOrder = object.value(QStringLiteral("sortOrder")).toInt();

    entry.name = object.value(QStringLiteral("name")).toString();
    entry.race = object.value(QStringLiteral("race")).toString();
    entry.characterClass = object.value(QStringLiteral("characterClass")).toString();
    entry.alignment = object.value(QStringLiteral("alignment")).toString();
    entry.size = object.value(QStringLiteral("size")).toString(QStringLiteral("Средний"));
    entry.creatureType = object.value(QStringLiteral("creatureType")).toString();
    entry.challengeRating = object.value(QStringLiteral("challengeRating")).toString();
    entry.experienceReward = qMax(0, object.value(QStringLiteral("experienceReward")).toInt(0));
    entry.level = qMax(1, object.value(QStringLiteral("level")).toInt(1));

    entry.strength = object.value(QStringLiteral("strength")).toInt(10);
    entry.dexterity = object.value(QStringLiteral("dexterity")).toInt(10);
    entry.constitution = object.value(QStringLiteral("constitution")).toInt(10);
    entry.intelligence = object.value(QStringLiteral("intelligence")).toInt(10);
    entry.wisdom = object.value(QStringLiteral("wisdom")).toInt(10);
    entry.charisma = object.value(QStringLiteral("charisma")).toInt(10);

    entry.armorClass = object.value(QStringLiteral("armorClass")).toInt(10);
    entry.maxHp = object.value(QStringLiteral("maxHp")).toInt(10);
    entry.currentHp = object.value(QStringLiteral("currentHp")).toInt(entry.maxHp);
    entry.speed = object.value(QStringLiteral("speed")).toInt(30);
    entry.initiative = object.value(QStringLiteral("initiative")).toInt();
    entry.proficiencyBonus = object.value(QStringLiteral("proficiencyBonus")).toInt(2);
    entry.passivePerception = qMax(1, object.value(QStringLiteral("passivePerception")).toInt(10));
    entry.armorDescription = object.value(QStringLiteral("armorDescription")).toString();
    entry.armorSource = object.value(QStringLiteral("armorSource")).toString();
    entry.shieldSource = object.value(QStringLiteral("shieldSource")).toString();
    entry.hitDiceFormula = object.value(QStringLiteral("hitDiceFormula")).toString();
    entry.speedDescription = object.value(QStringLiteral("speedDescription")).toString();
    entry.senses = object.value(QStringLiteral("senses")).toString();
    entry.damageVulnerabilities = object.value(QStringLiteral("damageVulnerabilities")).toString();
    entry.damageResistances = object.value(QStringLiteral("damageResistances")).toString();
    entry.damageImmunities = object.value(QStringLiteral("damageImmunities")).toString();
    entry.conditionImmunities = object.value(QStringLiteral("conditionImmunities")).toString();

    entry.appearance = object.value(QStringLiteral("appearance")).toString();
    entry.age = object.value(QStringLiteral("age")).toString();
    entry.height = object.value(QStringLiteral("height")).toString();
    entry.weight = object.value(QStringLiteral("weight")).toString();
    entry.skin = object.value(QStringLiteral("skin")).toString();
    entry.hair = object.value(QStringLiteral("hair")).toString();

    entry.description = object.value(QStringLiteral("description")).toString();
    entry.notes = object.value(QStringLiteral("notes")).toString();

    entry.languages = jsonToStringList(object.value(QStringLiteral("languages")));
    entry.skillProficiencies = jsonToStringList(object.value(QStringLiteral("skillProficiencies")));
    entry.savingThrowProficiencies = jsonToStringList(object.value(QStringLiteral("savingThrowProficiencies")));
    entry.armorProficiencies = jsonToStringList(object.value(QStringLiteral("armorProficiencies")));
    entry.weaponProficiencies = jsonToStringList(object.value(QStringLiteral("weaponProficiencies")));
    entry.attacks = jsonToStringList(object.value(QStringLiteral("attacks")));
    entry.traits = jsonToStringMap(object.value(QStringLiteral("traits")));

    entry.spells = jsonToSpellList(object.value(QStringLiteral("spells")));
    entry.inventory = jsonToItemList(object.value(QStringLiteral("inventory")));
    entry.characterRulesJson = object.value(QStringLiteral("characterRules")).toObject();

    if (entry.listName.trimmed().isEmpty()) {
        entry.listName = entry.name.trimmed().isEmpty() ? QStringLiteral("Новый NPC") : entry.name.trimmed();
    }
    if (entry.id.trimmed().isEmpty()) {
        entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    return entry;
}

QJsonObject NpcEntry::toJson() const
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), id);
    object.insert(QStringLiteral("listName"), listName);
    object.insert(QStringLiteral("folderId"), folderId);
    object.insert(QStringLiteral("sortOrder"), sortOrder);

    object.insert(QStringLiteral("name"), name);
    object.insert(QStringLiteral("race"), race);
    object.insert(QStringLiteral("characterClass"), characterClass);
    object.insert(QStringLiteral("alignment"), alignment);
    object.insert(QStringLiteral("size"), size);
    object.insert(QStringLiteral("creatureType"), creatureType);
    object.insert(QStringLiteral("challengeRating"), challengeRating);
    object.insert(QStringLiteral("experienceReward"), experienceReward);
    object.insert(QStringLiteral("level"), level);

    object.insert(QStringLiteral("strength"), strength);
    object.insert(QStringLiteral("dexterity"), dexterity);
    object.insert(QStringLiteral("constitution"), constitution);
    object.insert(QStringLiteral("intelligence"), intelligence);
    object.insert(QStringLiteral("wisdom"), wisdom);
    object.insert(QStringLiteral("charisma"), charisma);

    object.insert(QStringLiteral("armorClass"), armorClass);
    object.insert(QStringLiteral("maxHp"), maxHp);
    object.insert(QStringLiteral("currentHp"), currentHp);
    object.insert(QStringLiteral("speed"), speed);
    object.insert(QStringLiteral("initiative"), initiative);
    object.insert(QStringLiteral("proficiencyBonus"), proficiencyBonus);
    object.insert(QStringLiteral("passivePerception"), passivePerception);
    object.insert(QStringLiteral("armorDescription"), armorDescription);
    object.insert(QStringLiteral("armorSource"), armorSource);
    object.insert(QStringLiteral("shieldSource"), shieldSource);
    object.insert(QStringLiteral("hitDiceFormula"), hitDiceFormula);
    object.insert(QStringLiteral("speedDescription"), speedDescription);
    object.insert(QStringLiteral("senses"), senses);
    object.insert(QStringLiteral("damageVulnerabilities"), damageVulnerabilities);
    object.insert(QStringLiteral("damageResistances"), damageResistances);
    object.insert(QStringLiteral("damageImmunities"), damageImmunities);
    object.insert(QStringLiteral("conditionImmunities"), conditionImmunities);

    object.insert(QStringLiteral("appearance"), appearance);
    object.insert(QStringLiteral("age"), age);
    object.insert(QStringLiteral("height"), height);
    object.insert(QStringLiteral("weight"), weight);
    object.insert(QStringLiteral("skin"), skin);
    object.insert(QStringLiteral("hair"), hair);

    object.insert(QStringLiteral("description"), description);
    object.insert(QStringLiteral("notes"), notes);

    object.insert(QStringLiteral("languages"), stringListToJson(languages));
    object.insert(QStringLiteral("skillProficiencies"), stringListToJson(skillProficiencies));
    object.insert(QStringLiteral("savingThrowProficiencies"), stringListToJson(savingThrowProficiencies));
    object.insert(QStringLiteral("armorProficiencies"), stringListToJson(armorProficiencies));
    object.insert(QStringLiteral("weaponProficiencies"), stringListToJson(weaponProficiencies));
    object.insert(QStringLiteral("attacks"), stringListToJson(attacks));
    object.insert(QStringLiteral("traits"), stringMapToJson(traits));
    object.insert(QStringLiteral("spells"), spellListToJson(spells));
    object.insert(QStringLiteral("inventory"), itemListToJson(inventory));
    if (!characterRulesJson.isEmpty()) {
        object.insert(QStringLiteral("characterRules"), characterRulesJson);
    }
    return object;
}

NpcStorageData NpcStorageData::loadFromFile(const QString &path)
{
    NpcStorageData data;
    if (path.isEmpty() || !QFile::exists(path)) {
        return data;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return data;
    }

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    for (const QJsonValue &value : root.value(QStringLiteral("folders")).toArray()) {
        NpcFolder folder = NpcFolder::fromJson(value.toObject());
        if (!folder.id.isEmpty()) {
            data.folders.insert(folder.id, folder);
        }
    }
    for (const QJsonValue &value : root.value(QStringLiteral("npcs")).toArray()) {
        NpcEntry entry = NpcEntry::fromJson(value.toObject());
        if (!entry.id.isEmpty()) {
            data.npcs.insert(entry.id, entry);
        }
    }
    return data;
}

namespace {

QString abilityLine(const QString &label, int score)
{
    const int mod = Character::abilityModifier(score);
    return QStringLiteral("%1 %2 (%3%4)")
        .arg(label)
        .arg(score)
        .arg(mod >= 0 ? "+" : "")
        .arg(mod);
}

QString joinLines(const QStringList &values)
{
    return values.isEmpty() ? QStringLiteral("—") : values.join(QStringLiteral(", "));
}

} // namespace

QString formatNpcSummary(const NpcEntry &entry)
{
    QStringList lines;
    const QString displayName = entry.name.trimmed().isEmpty() ? entry.listName : entry.name;

    lines << QStringLiteral("=== %1 ===").arg(displayName);
    lines << QString();

    QStringList identity;
    const QString typeLine = QStringLiteral("%1 %2")
                                 .arg(entry.size.trimmed().isEmpty() ? QStringLiteral("Средний") : entry.size.trimmed())
                                 .arg(entry.creatureType.trimmed().isEmpty() ? QStringLiteral("гуманоид") : entry.creatureType.trimmed())
                                 .trimmed();
    identity << typeLine;
    if (!entry.alignment.trimmed().isEmpty()) {
        identity << entry.alignment.trimmed();
    }
    if (!entry.race.trimmed().isEmpty()) {
        identity << QStringLiteral("Раса: %1").arg(entry.race.trimmed());
    }
    if (!entry.characterClass.trimmed().isEmpty()) {
        identity << QStringLiteral("Класс: %1").arg(entry.characterClass.trimmed());
    }
    if (entry.level > 0) {
        identity << QStringLiteral("Уровень: %1").arg(entry.level);
    }
    if (!identity.isEmpty()) {
        lines << identity.join(QStringLiteral(" | "));
        lines << QString();
    }

    lines << QStringLiteral("Блок боя:");
    lines << QStringLiteral("Класс Доспеха: %1%2")
                 .arg(entry.armorClass)
                 .arg(entry.armorDescription.trimmed().isEmpty()
                          ? QString()
                          : QStringLiteral(" (%1)").arg(entry.armorDescription.trimmed()));
    if (!entry.armorSource.trimmed().isEmpty() || !entry.shieldSource.trimmed().isEmpty()) {
        QStringList acSources;
        if (!entry.armorSource.trimmed().isEmpty()) {
            acSources << entry.armorSource.trimmed();
        }
        if (!entry.shieldSource.trimmed().isEmpty()
            && entry.shieldSource.compare(QStringLiteral("Без щита"), Qt::CaseInsensitive) != 0) {
            acSources << entry.shieldSource.trimmed();
        }
        if (!acSources.isEmpty()) {
            lines << QStringLiteral("Источники КД: %1").arg(acSources.join(QStringLiteral(", ")));
        }
    }
    lines << QStringLiteral("Хиты: %1/%2%3")
                 .arg(entry.currentHp)
                 .arg(entry.maxHp)
                 .arg(entry.hitDiceFormula.trimmed().isEmpty()
                          ? QString()
                          : QStringLiteral(" (%1)").arg(entry.hitDiceFormula.trimmed()));
    lines << QStringLiteral("Скорость: %1")
                 .arg(entry.speedDescription.trimmed().isEmpty()
                          ? QStringLiteral("%1 фт.").arg(entry.speed)
                          : entry.speedDescription.trimmed());
    lines << QStringLiteral("Инициатива: %1 | Бонус мастерства: +%2")
                 .arg(entry.initiative >= 0 ? QStringLiteral("+%1").arg(entry.initiative)
                                            : QString::number(entry.initiative))
                 .arg(entry.proficiencyBonus);
    lines << QString();

    lines << QStringLiteral("Характеристики:");
    lines << abilityLine(QStringLiteral("СИЛ"), entry.strength);
    lines << abilityLine(QStringLiteral("ЛОВ"), entry.dexterity);
    lines << abilityLine(QStringLiteral("ТЕЛ"), entry.constitution);
    lines << abilityLine(QStringLiteral("ИНТ"), entry.intelligence);
    lines << abilityLine(QStringLiteral("МДР"), entry.wisdom);
    lines << abilityLine(QStringLiteral("ХАР"), entry.charisma);
    lines << QString();

    if (!entry.senses.trimmed().isEmpty() || entry.passivePerception > 0) {
        QString sensesLine = entry.senses.trimmed();
        if (entry.passivePerception > 0) {
            if (!sensesLine.isEmpty()) {
                sensesLine += QStringLiteral(", ");
            }
            sensesLine += QStringLiteral("пассивное Восприятие %1").arg(entry.passivePerception);
        }
        lines << QStringLiteral("Чувства: %1").arg(sensesLine);
    }
    if (!entry.damageVulnerabilities.trimmed().isEmpty()) {
        lines << QStringLiteral("Уязвимости к урону: %1").arg(entry.damageVulnerabilities.trimmed());
    }
    if (!entry.damageResistances.trimmed().isEmpty()) {
        lines << QStringLiteral("Сопротивления урону: %1").arg(entry.damageResistances.trimmed());
    }
    if (!entry.damageImmunities.trimmed().isEmpty()) {
        lines << QStringLiteral("Иммунитеты к урону: %1").arg(entry.damageImmunities.trimmed());
    }
    if (!entry.conditionImmunities.trimmed().isEmpty()) {
        lines << QStringLiteral("Иммунитеты к состояниям: %1").arg(entry.conditionImmunities.trimmed());
    }
    lines << QStringLiteral("Языки: %1").arg(joinLines(entry.languages));
    if (!entry.challengeRating.trimmed().isEmpty()) {
        lines << (entry.experienceReward > 0
                      ? QStringLiteral("Опасность: %1 (%2 опыта)").arg(entry.challengeRating.trimmed()).arg(entry.experienceReward)
                      : QStringLiteral("Опасность: %1").arg(entry.challengeRating.trimmed()));
    }
    lines << QString();

    QStringList appearance;
    if (!entry.age.trimmed().isEmpty()) {
        appearance << QStringLiteral("Возраст: %1").arg(entry.age.trimmed());
    }
    if (!entry.height.trimmed().isEmpty()) {
        appearance << QStringLiteral("Рост: %1").arg(entry.height.trimmed());
    }
    if (!entry.weight.trimmed().isEmpty()) {
        appearance << QStringLiteral("Вес: %1").arg(entry.weight.trimmed());
    }
    if (!entry.skin.trimmed().isEmpty()) {
        appearance << QStringLiteral("Кожа: %1").arg(entry.skin.trimmed());
    }
    if (!entry.hair.trimmed().isEmpty()) {
        appearance << QStringLiteral("Волосы: %1").arg(entry.hair.trimmed());
    }
    if (!appearance.isEmpty()) {
        lines << QStringLiteral("Внешность:");
        lines << appearance.join(QStringLiteral(" | "));
        if (!entry.appearance.trimmed().isEmpty()) {
            lines << entry.appearance.trimmed();
        }
        lines << QString();
    } else if (!entry.appearance.trimmed().isEmpty()) {
        lines << QStringLiteral("Внешность:");
        lines << entry.appearance.trimmed();
        lines << QString();
    }

    if (!entry.description.trimmed().isEmpty()) {
        lines << QStringLiteral("Описание:");
        lines << entry.description.trimmed();
        lines << QString();
    }

    lines << QStringLiteral("Владения:");
    lines << QStringLiteral("Навыки: %1").arg(joinLines(entry.skillProficiencies));
    lines << QStringLiteral("Спасброски: %1").arg(joinLines(entry.savingThrowProficiencies));
    lines << QStringLiteral("Доспехи: %1").arg(joinLines(entry.armorProficiencies));
    lines << QStringLiteral("Оружие: %1").arg(joinLines(entry.weaponProficiencies));
    lines << QString();

    if (!entry.attacks.isEmpty()) {
        lines << QStringLiteral("Действия:");
        for (const QString &attack : entry.attacks) {
            lines << QStringLiteral("- %1").arg(attack);
        }
        lines << QString();
    }

    if (!entry.traits.isEmpty()) {
        lines << QStringLiteral("Особенности:");
        for (auto it = entry.traits.begin(); it != entry.traits.end(); ++it) {
            if (it.value().trimmed().isEmpty()) {
                lines << QStringLiteral("- %1").arg(it.key());
            } else {
                lines << QStringLiteral("- %1: %2").arg(it.key(), it.value().trimmed());
            }
        }
        lines << QString();
    }

    if (!entry.spells.isEmpty()) {
        lines << QStringLiteral("Заклинания:");
        for (const Spell &spell : entry.spells) {
            if (spell.level > 0) {
                lines << QStringLiteral("- %1 (ур. %2)").arg(spell.name, QString::number(spell.level));
            } else {
                lines << QStringLiteral("- %1").arg(spell.name);
            }
        }
        lines << QString();
    }

    if (!entry.inventory.isEmpty()) {
        lines << QStringLiteral("Инвентарь:");
        for (const Item &item : entry.inventory) {
            if (item.quantity > 1) {
                lines << QStringLiteral("- %1 x%2").arg(item.name).arg(item.quantity);
            } else {
                lines << QStringLiteral("- %1").arg(item.name);
            }
        }
        lines << QString();
    }

    if (!entry.notes.trimmed().isEmpty()) {
        lines << QStringLiteral("Заметки мастера:");
        lines << entry.notes.trimmed();
    }

    return lines.join(QStringLiteral("\n"));
}

void syncCharacterFromNpcEntry(const NpcEntry &entry, Character &character)
{
    if (!entry.characterRulesJson.isEmpty()) {
        character.fromJson(entry.characterRulesJson);
    }

    character.setName(entry.name.isEmpty() ? entry.listName : entry.name);
    character.setRace(entry.race);
    character.setCharacterClass(entry.characterClass);
    character.level = entry.level;
    character.alignment = entry.alignment;
    character.size = entry.size;

    character.strength = entry.strength;
    character.dexterity = entry.dexterity;
    character.constitution = entry.constitution;
    character.intelligence = entry.intelligence;
    character.wisdom = entry.wisdom;
    character.charisma = entry.charisma;

    character.armorClass = entry.armorClass;
    character.maxHp = entry.maxHp;
    character.currentHp = entry.currentHp;
    character.speed = entry.speed;
    character.initiative = entry.initiative;
    character.proficiencyBonus = entry.proficiencyBonus;

    character.age = entry.age;
    character.height = entry.height;
    character.weight = entry.weight;
    character.skin = entry.skin;
    character.hair = entry.hair;
    character.appearance = entry.appearance;
    character.personalHistory = entry.description;

    character.languages = entry.languages;
    character.skillProficiencies = entry.skillProficiencies;
    character.savingThrowProficiencies = entry.savingThrowProficiencies;
    character.armorProficiencies = entry.armorProficiencies;
    character.weaponProficiencies = entry.weaponProficiencies;
    character.attacks = entry.attacks;
    character.traits = entry.traits;
    character.spells = entry.spells;
    character.inventory = entry.inventory;
}

void syncNpcEntryFromCharacter(const Character &character, NpcEntry &entry)
{
    entry.characterRulesJson = character.toJson();
    entry.name = character.name();
    entry.race = character.race();
    entry.characterClass = character.characterClass();
    entry.level = character.level;
    entry.alignment = character.alignment;
    entry.size = character.size;

    entry.strength = character.strength;
    entry.dexterity = character.dexterity;
    entry.constitution = character.constitution;
    entry.intelligence = character.intelligence;
    entry.wisdom = character.wisdom;
    entry.charisma = character.charisma;

    entry.armorClass = character.armorClass;
    entry.maxHp = character.maxHp;
    entry.currentHp = character.currentHp;
    entry.speed = character.speed;
    entry.initiative = character.initiative;
    entry.proficiencyBonus = character.proficiencyBonus;
    if (entry.size.trimmed().isEmpty()) {
        entry.size = character.size.trimmed().isEmpty() ? QStringLiteral("Средний") : character.size.trimmed();
    }
    if (entry.speedDescription.trimmed().isEmpty() && character.speed > 0) {
        entry.speedDescription = QStringLiteral("%1 фт.").arg(character.speed);
    }
    const bool hasPerception = character.skillProficiencies.contains(QStringLiteral("Восприятие"), Qt::CaseInsensitive);
    entry.passivePerception = qMax(
        1,
        10 + Character::abilityModifier(character.wisdom) + (hasPerception ? character.proficiencyBonus : 0));

    entry.age = character.age;
    entry.height = character.height;
    entry.weight = character.weight;
    entry.skin = character.skin;
    entry.hair = character.hair;
    entry.appearance = character.appearance;
    entry.description = character.personalHistory;

    entry.languages = character.languages;
    entry.skillProficiencies = character.skillProficiencies;
    entry.savingThrowProficiencies = character.savingThrowProficiencies;
    entry.armorProficiencies = character.armorProficiencies;
    entry.weaponProficiencies = character.weaponProficiencies;
    entry.attacks = character.attacks;
    entry.traits = character.traits;
    entry.spells = character.spells;
    entry.inventory = character.inventory;
}
