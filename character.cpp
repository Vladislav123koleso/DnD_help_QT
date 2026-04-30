#include "character.h"
#include "databasemanager.h"
#include <QJsonArray>
#include <QRegularExpression>
#include <QtGlobal>
#include <cmath>

namespace {

QString normalizedName(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QStringLiteral(" "));
    value.replace(QStringLiteral("ё"), QStringLiteral("е"));
    return value.simplified().toLower();
}

bool containsByName(const QStringList &values, const QString &needle)
{
    const QString normalizedNeedle = normalizedName(needle);
    for (const QString &value : values) {
        if (normalizedName(value) == normalizedNeedle) {
            return true;
        }
    }
    return false;
}

QString cleanedTraitTitle(QString title)
{
    title = title.trimmed();
    while (title.startsWith(QStringLiteral("!"))) {
        title.remove(0, 1);
        title = title.trimmed();
    }
    if (title.startsWith(QStringLiteral("(заклинание)"), Qt::CaseInsensitive)) {
        title.remove(0, QStringLiteral("(заклинание)").size());
        title = title.trimmed();
    }
    return title.simplified();
}

bool listContainsFragment(const QStringList &values, const QString &fragment)
{
    const QString normalizedFragment = normalizedName(fragment);
    for (const QString &value : values) {
        if (normalizedName(value).contains(normalizedFragment)) {
            return true;
        }
    }
    return false;
}

int abilityScoreByName(const Character *character, const QString &abilityName)
{
    if (!character) {
        return 10;
    }
    const QString normalizedAbility = normalizedName(abilityName);
    if (normalizedAbility.contains(QStringLiteral("сил"))) {
        return character->strength;
    }
    if (normalizedAbility.contains(QStringLiteral("ловк"))) {
        return character->dexterity;
    }
    if (normalizedAbility.contains(QStringLiteral("телослож"))) {
        return character->constitution;
    }
    if (normalizedAbility.contains(QStringLiteral("интеллект"))) {
        return character->intelligence;
    }
    if (normalizedAbility.contains(QStringLiteral("мудр"))) {
        return character->wisdom;
    }
    if (normalizedAbility.contains(QStringLiteral("харизм"))) {
        return character->charisma;
    }
    return 10;
}

struct ArmorClassCandidate {
    int value = 10;
    bool allowShield = true;
};

struct EquippedArmorState {
    bool wearingArmor = false;
    bool wearingMediumArmor = false;
    bool wearingHeavyArmor = false;
    bool usingShield = false;
    int shieldBonus = 0;
    ArmorClassCandidate armorCandidate;
};

int computedArmorFormulaValue(const Character *character, int baseValue, const QString &abilityName, int abilityCap)
{
    int value = baseValue;
    if (!abilityName.trimmed().isEmpty()) {
        int modifier = Character::abilityModifier(abilityScoreByName(character, abilityName));
        if (abilityCap >= 0) {
            modifier = qMin(modifier, abilityCap);
        }
        value += modifier;
    }
    return value;
}

bool parseArmorFormula(const QString &text, int *baseValue, QString *abilityName, int *abilityCap)
{
    const QRegularExpression regex(
        QStringLiteral("КД\\s*:\\s*([+-]?\\d+)\\s*(?:\\+\\s*модификатор\\s*(Силы|Ловкости|Телосложения|Интеллекта|Мудрости|Харизмы))?(?:\\s*\\((?:макс\\.?|максимум)\\s*(\\d+)\\))?"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = regex.match(text);
    if (!match.hasMatch()) {
        return false;
    }

    if (baseValue) {
        *baseValue = match.captured(1).toInt();
    }
    if (abilityName) {
        *abilityName = match.captured(2).trimmed();
    }
    if (abilityCap) {
        *abilityCap = match.captured(3).isEmpty() ? -1 : match.captured(3).toInt();
    }
    return true;
}

bool parseTraitArmorFormula(const QString &text, int *baseValue, QString *abilityName)
{
    const QRegularExpression formulaRegex(
        QStringLiteral("(?:класс\\s+доспеха|кд)(?:[^\\d]{0,40})?(?:равен|составляет|дарует\\s+вам|дает\\s+вам|даёт\\s+вам)?\\s*(\\d+)\\s*\\+\\s*(?:ваш\\s*)?модификатор\\s*(Силы|Ловкости|Телосложения|Интеллекта|Мудрости|Харизмы)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch formulaMatch = formulaRegex.match(text);
    if (formulaMatch.hasMatch()) {
        if (baseValue) {
            *baseValue = formulaMatch.captured(1).toInt();
        }
        if (abilityName) {
            *abilityName = formulaMatch.captured(2).trimmed();
        }
        return true;
    }

    const QRegularExpression fixedRegex(
        QStringLiteral("(?:базовый\\s*)?(?:класс\\s+доспеха|кд)(?:[^\\d]{0,40})?(?:равен|составляет|дарует\\s+вам|дает\\s+вам|даёт\\s+вам)?\\s*(\\d+)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch fixedMatch = fixedRegex.match(text);
    if (!fixedMatch.hasMatch()) {
        return false;
    }

    if (baseValue) {
        *baseValue = fixedMatch.captured(1).toInt();
    }
    if (abilityName) {
        abilityName->clear();
    }
    return true;
}

EquippedArmorState equippedArmorState(const Character *character)
{
    EquippedArmorState state;
    state.armorCandidate.value = 10 + Character::abilityModifier(character ? character->dexterity : 10);

    if (!character) {
        return state;
    }

    bool armorResolved = false;
    for (const Item &item : character->inventory) {
        if (!item.isEquipped) {
            continue;
        }

        const QString normalizedType = normalizedName(item.type);
        if (normalizedType.contains(QStringLiteral("щит"))) {
            state.usingShield = true;
            if (listContainsFragment(character->armorProficiencies, QStringLiteral("щит"))) {
                int shieldBonus = 2;
                int parsedShieldBonus = 0;
                QString unusedAbility;
                int unusedCap = -1;
                if (parseArmorFormula(item.description, &parsedShieldBonus, &unusedAbility, &unusedCap) && parsedShieldBonus > 0) {
                    shieldBonus = parsedShieldBonus;
                }
                state.shieldBonus = qMax(state.shieldBonus, shieldBonus);
            }
            continue;
        }

        if (armorResolved || !normalizedType.contains(QStringLiteral("доспех"))) {
            continue;
        }

        state.wearingArmor = true;
        state.wearingMediumArmor = normalizedType.contains(QStringLiteral("средн"));
        state.wearingHeavyArmor = normalizedType.contains(QStringLiteral("тяж"));

        int baseValue = 10;
        QString abilityName;
        int abilityCap = -1;
        if (parseArmorFormula(item.description, &baseValue, &abilityName, &abilityCap)) {
            state.armorCandidate.value = computedArmorFormulaValue(character, baseValue, abilityName, abilityCap);
        }
        armorResolved = true;
    }

    return state;
}

QList<ArmorClassCandidate> raceArmorCandidates(const Character *character, const EquippedArmorState &state)
{
    QList<ArmorClassCandidate> candidates;
    if (!character) {
        return candidates;
    }

    for (auto it = character->traits.begin(); it != character->traits.end(); ++it) {
        const QString title = cleanedTraitTitle(it.key());
        const QString description = it.value().trimmed();
        const QString normalizedDescription = normalizedName(description);
        if (title.isEmpty() || description.isEmpty()) {
            continue;
        }
        if (!normalizedDescription.contains(QStringLiteral("класс доспеха")) &&
            !normalizedDescription.contains(QStringLiteral(" кд ")) &&
            !normalizedDescription.startsWith(QStringLiteral("кд"))) {
            continue;
        }

        int baseValue = 0;
        QString abilityName;
        if (!parseTraitArmorFormula(description, &baseValue, &abilityName)) {
            continue;
        }

        const bool requiresNoArmorAndShield = normalizedDescription.contains(QStringLiteral("не носите ни доспех")) &&
                                              normalizedDescription.contains(QStringLiteral("ни щит"));
        const bool requiresNoArmor = normalizedDescription.contains(QStringLiteral("не носите доспех")) ||
                                     normalizedDescription.contains(QStringLiteral("не носите брони")) ||
                                     normalizedDescription.contains(QStringLiteral("пока вы не носите доспех"));
        const bool usableIfArmorIsWorse = normalizedDescription.contains(QStringLiteral("если доспех")) &&
                                         (normalizedDescription.contains(QStringLiteral("более низкий кд")) ||
                                          normalizedDescription.contains(QStringLiteral("меньший кд")));
        const bool forbidsArmor = normalizedDescription.contains(QStringLiteral("не можете носить доспех"));

        if (requiresNoArmorAndShield && (state.wearingArmor || state.usingShield)) {
            continue;
        }
        if (requiresNoArmor && state.wearingArmor && !usableIfArmorIsWorse && !forbidsArmor) {
            continue;
        }

        ArmorClassCandidate candidate;
        candidate.value = computedArmorFormulaValue(character, baseValue, abilityName, -1);
        candidate.allowShield = !requiresNoArmorAndShield;
        candidates.append(candidate);
    }

    return candidates;
}

QList<ArmorClassCandidate> classArmorCandidates(const Character *character, const EquippedArmorState &state)
{
    QList<ArmorClassCandidate> candidates;
    if (!character || character->classLevels.isEmpty()) {
        return candidates;
    }

    const QList<Class> classes = DatabaseManager::instance().getAllClasses();
    for (auto classIt = character->classLevels.begin(); classIt != character->classLevels.end(); ++classIt) {
        const QString className = classIt.key().trimmed();
        const int classLevel = classIt.value();
        if (className.isEmpty() || classLevel <= 0) {
            continue;
        }

        Class cls;
        bool foundClass = false;
        for (const Class &candidateClass : classes) {
            if (normalizedName(candidateClass.name) == normalizedName(className)) {
                cls = candidateClass;
                foundClass = true;
                break;
            }
        }
        if (!foundClass) {
            continue;
        }

        for (const ClassSection &section : cls.featureSections) {
            if (section.levelRequirement > classLevel) {
                continue;
            }

            const QString normalizedTitle = normalizedName(section.title);
            const QString normalizedDescription = normalizedName(section.description);
            if (normalizedTitle != normalizedName(QStringLiteral("Защита без доспехов"))) {
                continue;
            }

            if (state.wearingArmor) {
                continue;
            }

            const bool allowShield = normalizedDescription.contains(normalizedName(QStringLiteral("можете использовать щит")));
            if (!allowShield && state.usingShield) {
                continue;
            }

            ArmorClassCandidate candidate;
            candidate.value = 10 + Character::abilityModifier(character->dexterity);
            if (normalizedDescription.contains(normalizedName(QStringLiteral("Телосложения")))) {
                candidate.value += Character::abilityModifier(character->constitution);
            }
            if (normalizedDescription.contains(normalizedName(QStringLiteral("Мудрости")))) {
                candidate.value += Character::abilityModifier(character->wisdom);
            }
            candidate.allowShield = allowShield;
            candidates.append(candidate);
        }
    }

    return candidates;
}

int passiveRaceArmorBonus(const Character *character, const EquippedArmorState &state)
{
    if (!character) {
        return 0;
    }

    int bonus = 0;
    const QRegularExpression bonusRegex(QStringLiteral("\\+(\\d+)\\s*к\\s*(?:классу\\s*доспеха|кд)"), QRegularExpression::CaseInsensitiveOption);

    for (auto it = character->traits.begin(); it != character->traits.end(); ++it) {
        const QString normalizedDescription = normalizedName(it.value());
        if (normalizedDescription.contains(QStringLiteral("действием")) ||
            normalizedDescription.contains(QStringLiteral("бонусным действием")) ||
            normalizedDescription.contains(QStringLiteral("реакцией")) ||
            normalizedDescription.contains(QStringLiteral("до конца")) ||
            normalizedDescription.contains(QStringLiteral("на 1 минут"))) {
            continue;
        }

        const QRegularExpressionMatch match = bonusRegex.match(it.value());
        if (!match.hasMatch()) {
            continue;
        }

        if (normalizedDescription.contains(QStringLiteral("не носите тяж")) && state.wearingHeavyArmor) {
            continue;
        }
        if (normalizedDescription.contains(QStringLiteral("не носите доспех")) && state.wearingArmor) {
            continue;
        }
        if (normalizedDescription.contains(QStringLiteral("не носите ни доспех")) && (state.wearingArmor || state.usingShield)) {
            continue;
        }

        bonus += match.captured(1).toInt();
    }

    return bonus;
}

int passiveClassArmorBonus(const Character *character, const EquippedArmorState &state)
{
    if (!character || character->classLevels.isEmpty()) {
        return 0;
    }

    int bonus = 0;
    const QList<Class> classes = DatabaseManager::instance().getAllClasses();
    for (auto classIt = character->classLevels.begin(); classIt != character->classLevels.end(); ++classIt) {
        const QString className = classIt.key().trimmed();
        const int classLevel = classIt.value();
        if (className.isEmpty() || classLevel <= 0) {
            continue;
        }

        for (const Class &cls : classes) {
            if (normalizedName(cls.name) != normalizedName(className)) {
                continue;
            }

            for (const ClassSection &section : cls.featureSections) {
                if (section.levelRequirement > classLevel) {
                    continue;
                }

                const QString normalizedDescription = normalizedName(section.description);
                if (normalizedDescription.contains(QStringLiteral("выберите"))) {
                    continue;
                }
                if (normalizedDescription.contains(QStringLiteral("когда вы носите тяжёлый доспех")) &&
                    normalizedDescription.contains(QStringLiteral("+1 к кд")) &&
                    state.wearingHeavyArmor) {
                    bonus += 1;
                }
            }
            break;
        }
    }

    return bonus;
}

QStringList jsonArrayToStringList(const QJsonValue &value)
{
    QStringList result;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        const QString text = entry.toString().trimmed();
        if (!text.isEmpty()) {
            result << text;
        }
    }
    result.removeDuplicates();
    return result;
}

QJsonArray stringListToJsonArray(const QStringList &values)
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

QJsonObject stringMapToJsonObject(const QMap<QString, QString> &map)
{
    QJsonObject object;
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (!it.key().trimmed().isEmpty()) {
            object.insert(it.key(), it.value());
        }
    }
    return object;
}

QMap<QString, QString> jsonObjectToStringMap(const QJsonValue &value)
{
    QMap<QString, QString> map;
    const QJsonObject object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
        map.insert(it.key(), it.value().toString());
    }
    return map;
}

QJsonObject intMapToJsonObject(const QMap<QString, int> &map)
{
    QJsonObject object;
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (!it.key().trimmed().isEmpty()) {
            object.insert(it.key(), it.value());
        }
    }
    return object;
}

QMap<QString, int> jsonObjectToIntMap(const QJsonValue &value)
{
    QMap<QString, int> map;
    const QJsonObject object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
        map.insert(it.key(), it.value().toInt());
    }
    return map;
}

QJsonObject spellToJson(const Spell &spell)
{
    QJsonObject object;
    object.insert("name", spell.name);
    object.insert("level", spell.level);
    object.insert("school", spell.school);
    object.insert("ritual", spell.ritual);
    object.insert("castingTime", spell.castingTime);
    object.insert("range", spell.range);
    object.insert("components", spell.components);
    object.insert("duration", spell.duration);
    object.insert("description", spell.description);
    object.insert("classes", spell.classes);
    object.insert("subclasses", spell.subclasses);
    object.insert("selectionClass", spell.selectionClass);
    object.insert("source", spell.source);
    object.insert("upper", spell.upper);
    return object;
}

Spell jsonToSpell(const QJsonValue &value)
{
    const QJsonObject object = value.toObject();
    Spell spell;
    spell.name = object.value("name").toString();
    spell.level = object.value("level").toInt();
    spell.school = object.value("school").toString();
    spell.ritual = object.value("ritual").toBool();
    spell.castingTime = object.value("castingTime").toString();
    spell.range = object.value("range").toString();
    spell.components = object.value("components").toString();
    spell.duration = object.value("duration").toString();
    spell.description = object.value("description").toString();
    spell.classes = object.value("classes").toString();
    spell.subclasses = object.value("subclasses").toString();
    spell.selectionClass = object.value("selectionClass").toString();
    spell.source = object.value("source").toString();
    spell.upper = object.value("upper").toString();
    return spell;
}

QJsonObject itemToJson(const Item &item)
{
    QJsonObject object;
    object.insert("id", item.id);
    object.insert("name", item.name);
    object.insert("nameEng", item.nameEng);
    object.insert("type", item.type);
    object.insert("rarity", item.rarity);
    object.insert("cost", item.cost);
    object.insert("weight", item.weight);
    object.insert("source", item.source);
    object.insert("description", item.description);
    object.insert("quantity", item.quantity);
    object.insert("isEquipped", item.isEquipped);
    return object;
}

Item jsonToItem(const QJsonValue &value)
{
    const QJsonObject object = value.toObject();
    Item item;
    item.id = object.value("id").toInt(-1);
    item.name = object.value("name").toString();
    item.nameEng = object.value("nameEng").toString();
    item.type = object.value("type").toString();
    item.rarity = object.value("rarity").toString();
    item.cost = object.value("cost").toString();
    item.weight = object.value("weight").toString();
    item.source = object.value("source").toString();
    item.description = object.value("description").toString();
    item.quantity = object.value("quantity").toInt(1);
    item.isEquipped = object.value("isEquipped").toBool(false);
    return item;
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
    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        spells.append(jsonToSpell(entry));
    }
    return spells;
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
    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        items.append(jsonToItem(entry));
    }
    return items;
}

QStringList uniqueStrings(QStringList values)
{
    for (QString &value : values) {
        value = value.trimmed();
    }
    values.removeAll(QString());
    values.removeDuplicates();
    return values;
}

int averageHitPointGain(int hitDie)
{
    return qMax(1, hitDie / 2 + 1);
}

}

Character::Character(QObject *parent) : QObject(parent)
{
    resetToDefaults();
}

int Character::abilityModifier(int score)
{
    return static_cast<int>(std::floor((score - 10) / 2.0));
}

int Character::clampAbilityScore(int score)
{
    return qBound(1, score, 20);
}

int Character::proficiencyBonusForLevel(int level)
{
    const int safeLevel = qBound(1, level, 20);
    return 2 + ((safeLevel - 1) / 4);
}

int Character::experienceForLevel(int level)
{
    static const int thresholds[] = {
        0, 300, 900, 2700, 6500,
        14000, 23000, 34000, 48000, 64000,
        85000, 100000, 120000, 140000, 165000,
        195000, 225000, 265000, 305000, 355000
    };

    const int safeLevel = qBound(1, level, 20);
    return thresholds[safeLevel - 1];
}

void Character::resetToDefaults()
{
    setName(QString());
    setCharacterClass(QString());
    setRace(QString());

    level = 1;
    strength = 10;
    dexterity = 10;
    constitution = 10;
    intelligence = 10;
    wisdom = 10;
    charisma = 10;

    maxHp = 0;
    currentHp = 0;
    tempHp = 0;
    armorClass = 10;
    initiative = 0;
    hitDie = 0;
    proficiencyBonus = 2;
    experiencePoints = 0;
    alignment.clear();

    background.clear();
    backgroundDescription.clear();
    backgroundFeatureName.clear();
    backgroundFeatureDescription.clear();
    personalHistory.clear();
    appearance.clear();
    age.clear();
    height.clear();
    weight.clear();
    skin.clear();
    hair.clear();

    speed = 30;
    flyingSpeed = 0;
    size = QStringLiteral("Средний");
    languages.clear();
    skillProficiencies.clear();
    toolProficiencies.clear();
    featNames.clear();
    abilityScoreImprovementLog.clear();
    traits.clear();
    featDescriptions.clear();
    savingThrowProficiencies.clear();
    armorProficiencies.clear();
    weaponProficiencies.clear();
    classLevels.clear();
    classHitDice.clear();
    classOrder.clear();

    attacks.clear();
    spellSlotCurrent.clear();
    spells.clear();
    spellbook.clear();
    inventory.clear();
}

void Character::recalculateDerivedStats(bool refillCurrentHp)
{
    level = qMax(1, level);
    proficiencyBonus = proficiencyBonusForLevel(level);
    experiencePoints = experienceForLevel(level);
    initiative = abilityModifier(dexterity);
    tempHp = qMax(0, tempHp);

    if (speed <= 0) {
        speed = 30;
    }
    if (size.trimmed().isEmpty()) {
        size = QStringLiteral("Средний");
    }

    languages = uniqueStrings(languages);
    skillProficiencies = uniqueStrings(skillProficiencies);
    toolProficiencies = uniqueStrings(toolProficiencies);
    featNames = uniqueStrings(featNames);
    savingThrowProficiencies = uniqueStrings(savingThrowProficiencies);
    armorProficiencies = uniqueStrings(armorProficiencies);
    weaponProficiencies = uniqueStrings(weaponProficiencies);
    attacks = uniqueStrings(attacks);

    const EquippedArmorState armorState = equippedArmorState(this);
    int bestArmorClass = 10 + abilityModifier(dexterity) + armorState.shieldBonus;
    if (armorState.wearingArmor) {
        bestArmorClass = qMax(bestArmorClass, armorState.armorCandidate.value + armorState.shieldBonus);
    }

    const QList<ArmorClassCandidate> raceCandidates = raceArmorCandidates(this, armorState);
    for (const ArmorClassCandidate &candidate : raceCandidates) {
        bestArmorClass = qMax(bestArmorClass, candidate.value + (candidate.allowShield ? armorState.shieldBonus : 0));
    }

    const QList<ArmorClassCandidate> classCandidates = classArmorCandidates(this, armorState);
    for (const ArmorClassCandidate &candidate : classCandidates) {
        bestArmorClass = qMax(bestArmorClass, candidate.value + (candidate.allowShield ? armorState.shieldBonus : 0));
    }

    armorClass = bestArmorClass + passiveRaceArmorBonus(this, armorState) + passiveClassArmorBonus(this, armorState);

    const int constitutionModifier = abilityModifier(constitution);
    int calculatedMaxHp = 0;
    bool firstClassProcessed = false;

    for (const QString &className : classOrder) {
        int classLevel = classLevels.value(className, 0);
        const int classHitDie = classHitDice.value(className, hitDie);

        if (classLevel <= 0 || classHitDie <= 0) {
            continue;
        }

        if (!firstClassProcessed) {
            hitDie = classHitDie;
            calculatedMaxHp += qMax(1, classHitDie + constitutionModifier);
            --classLevel;
            firstClassProcessed = true;
        }

        if (classLevel > 0) {
            calculatedMaxHp += classLevel * qMax(1, averageHitPointGain(classHitDie) + constitutionModifier);
        }
    }

    if (!firstClassProcessed && hitDie > 0) {
        calculatedMaxHp = qMax(1, hitDie + constitutionModifier);
        if (level > 1) {
            calculatedMaxHp += (level - 1) * qMax(1, averageHitPointGain(hitDie) + constitutionModifier);
        }
    }

    maxHp = calculatedMaxHp;
    if (refillCurrentHp || currentHp <= 0 || currentHp > maxHp) {
        currentHp = maxHp;
    }
}

QJsonObject Character::toJson() const
{
    QJsonObject obj;
    obj.insert("name", name());
    obj.insert("race", race());
    obj.insert("characterClass", characterClass());
    obj.insert("level", level);
    obj.insert("strength", strength);
    obj.insert("dexterity", dexterity);
    obj.insert("constitution", constitution);
    obj.insert("intelligence", intelligence);
    obj.insert("wisdom", wisdom);
    obj.insert("charisma", charisma);
    obj.insert("maxHp", maxHp);
    obj.insert("currentHp", currentHp);
    obj.insert("tempHp", tempHp);
    obj.insert("armorClass", armorClass);
    obj.insert("initiative", initiative);
    obj.insert("hitDie", hitDie);
    obj.insert("proficiencyBonus", proficiencyBonus);
    obj.insert("experiencePoints", experiencePoints);
    obj.insert("alignment", alignment);
    obj.insert("background", background);
    obj.insert("backgroundDescription", backgroundDescription);
    obj.insert("backgroundFeatureName", backgroundFeatureName);
    obj.insert("backgroundFeatureDescription", backgroundFeatureDescription);
    obj.insert("personalHistory", personalHistory);
    obj.insert("appearance", appearance);
    obj.insert("age", age);
    obj.insert("height", height);
    obj.insert("weight", weight);
    obj.insert("skin", skin);
    obj.insert("hair", hair);
    obj.insert("speed", speed);
    obj.insert("flyingSpeed", flyingSpeed);
    obj.insert("size", size);
    obj.insert("languages", stringListToJsonArray(languages));
    obj.insert("skillProficiencies", stringListToJsonArray(skillProficiencies));
    obj.insert("toolProficiencies", stringListToJsonArray(toolProficiencies));
    obj.insert("featNames", stringListToJsonArray(featNames));
    obj.insert("abilityScoreImprovementLog", stringListToJsonArray(abilityScoreImprovementLog));
    obj.insert("traits", stringMapToJsonObject(traits));
    obj.insert("featDescriptions", stringMapToJsonObject(featDescriptions));
    obj.insert("savingThrowProficiencies", stringListToJsonArray(savingThrowProficiencies));
    obj.insert("armorProficiencies", stringListToJsonArray(armorProficiencies));
    obj.insert("weaponProficiencies", stringListToJsonArray(weaponProficiencies));
    obj.insert("classLevels", intMapToJsonObject(classLevels));
    obj.insert("classHitDice", intMapToJsonObject(classHitDice));
    obj.insert("classOrder", stringListToJsonArray(classOrder));
    obj.insert("attacks", stringListToJsonArray(attacks));
    obj.insert("spellSlotCurrent", intMapToJsonObject(spellSlotCurrent));
    obj.insert("spells", spellListToJson(spells));
    obj.insert("spellbook", spellListToJson(spellbook));
    obj.insert("inventory", itemListToJson(inventory));
    return obj;
}

void Character::fromJson(const QJsonObject &obj)
{
    resetToDefaults();

    setName(obj.value("name").toString());
    setRace(obj.value("race").toString());
    setCharacterClass(obj.value("characterClass").toString());

    level = qMax(1, obj.value("level").toInt(1));
    strength = clampAbilityScore(obj.value("strength").toInt(10));
    dexterity = clampAbilityScore(obj.value("dexterity").toInt(10));
    constitution = clampAbilityScore(obj.value("constitution").toInt(10));
    intelligence = clampAbilityScore(obj.value("intelligence").toInt(10));
    wisdom = clampAbilityScore(obj.value("wisdom").toInt(10));
    charisma = clampAbilityScore(obj.value("charisma").toInt(10));

    currentHp = obj.value("currentHp").toInt(0);
    tempHp = obj.value("tempHp").toInt(0);
    hitDie = obj.value("hitDie").toInt(0);
    experiencePoints = obj.value("experiencePoints").toInt(experienceForLevel(level));
    alignment = obj.value("alignment").toString();

    background = obj.value("background").toString();
    backgroundDescription = obj.value("backgroundDescription").toString();
    backgroundFeatureName = obj.value("backgroundFeatureName").toString();
    backgroundFeatureDescription = obj.value("backgroundFeatureDescription").toString();
    personalHistory = obj.value("personalHistory").toString();
    appearance = obj.value("appearance").toString();
    age = obj.value("age").toString();
    height = obj.value("height").toString();
    weight = obj.value("weight").toString();
    skin = obj.value("skin").toString();
    hair = obj.value("hair").toString();

    speed = obj.value("speed").toInt(30);
    flyingSpeed = obj.value("flyingSpeed").toInt(0);
    size = obj.value("size").toString(QStringLiteral("Средний"));
    languages = jsonArrayToStringList(obj.value("languages"));
    skillProficiencies = jsonArrayToStringList(obj.value("skillProficiencies"));
    toolProficiencies = jsonArrayToStringList(obj.value("toolProficiencies"));
    featNames = jsonArrayToStringList(obj.value("featNames"));
    abilityScoreImprovementLog = jsonArrayToStringList(obj.value("abilityScoreImprovementLog"));
    traits = jsonObjectToStringMap(obj.value("traits"));
    featDescriptions = jsonObjectToStringMap(obj.value("featDescriptions"));
    savingThrowProficiencies = jsonArrayToStringList(obj.value("savingThrowProficiencies"));
    armorProficiencies = jsonArrayToStringList(obj.value("armorProficiencies"));
    weaponProficiencies = jsonArrayToStringList(obj.value("weaponProficiencies"));
    classLevels = jsonObjectToIntMap(obj.value("classLevels"));
    classHitDice = jsonObjectToIntMap(obj.value("classHitDice"));
    classOrder = jsonArrayToStringList(obj.value("classOrder"));
    attacks = jsonArrayToStringList(obj.value("attacks"));
    spellSlotCurrent = jsonObjectToIntMap(obj.value("spellSlotCurrent"));
    spells = jsonToSpellList(obj.value("spells"));
    spellbook = jsonToSpellList(obj.value("spellbook"));
    inventory = jsonToItemList(obj.value("inventory"));

    if (classOrder.isEmpty() && !classLevels.isEmpty()) {
        classOrder = classLevels.keys();
    }

    recalculateDerivedStats(false);
}

QString Character::name() const
{
    return m_name;
}

void Character::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged();
    }
}

QString Character::characterClass() const
{
    return m_characterClass;
}

void Character::setCharacterClass(const QString &characterClass)
{
    if (m_characterClass != characterClass) {
        m_characterClass = characterClass;
        emit characterClassChanged();
    }
}

QString Character::race() const
{
    return m_race;
}

void Character::setRace(const QString &race)
{
    if (m_race != race) {
        m_race = race;
        emit raceChanged();
    }
}
