#include "characterprogressionrules.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include "character.h"

namespace {

QString defaultLabelForMode(const QString &mode)
{
    if (mode == "prepared") {
        return QStringLiteral("подготовленных заклинаний");
    }
    if (mode == "spellbook_prepared") {
        return QStringLiteral("заклинаний в книге");
    }
    return QStringLiteral("известных заклинаний");
}

}

bool SpellSelectionRule::isValid() const
{
    return cantripLimit > 0 || leveledLimit > 0 || preparedLimit > 0 || maxSpellLevel > 0;
}

CharacterProgressionRules &CharacterProgressionRules::instance()
{
    static CharacterProgressionRules rules;
    return rules;
}

CharacterProgressionRules::CharacterProgressionRules()
{
}

int CharacterProgressionRules::characterAbilityScore(const Character *character, const QString &abilityName) const
{
    if (!character) {
        return 0;
    }

    if (abilityName == QStringLiteral("Сила")) {
        return character->strength;
    }
    if (abilityName == QStringLiteral("Ловкость")) {
        return character->dexterity;
    }
    if (abilityName == QStringLiteral("Телосложение")) {
        return character->constitution;
    }
    if (abilityName == QStringLiteral("Интеллект")) {
        return character->intelligence;
    }
    if (abilityName == QStringLiteral("Мудрость")) {
        return character->wisdom;
    }
    if (abilityName == QStringLiteral("Харизма")) {
        return character->charisma;
    }

    return 0;
}

QString CharacterProgressionRules::resolveRulesPath() const
{
    const QString relativePath = QStringLiteral("data/character_progression_rules.json");
    const QStringList candidatePaths = {
        QDir::current().filePath(relativePath),
        QDir(QCoreApplication::applicationDirPath()).filePath(relativePath),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../") + relativePath),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../") + relativePath),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../../../") + relativePath)
    };

    for (const QString &candidate : candidatePaths) {
        if (QFile::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }

    return QString();
}

void CharacterProgressionRules::ensureLoaded() const
{
    if (m_loaded) {
        return;
    }

    m_loaded = true;
    m_error.clear();
    m_featSlots = QJsonObject();
    m_spellcasting = QJsonObject();

    const QString path = resolveRulesPath();
    if (path.isEmpty()) {
        m_error = QStringLiteral("Не найден файл data/character_progression_rules.json");
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Не удалось открыть файл правил: %1").arg(path);
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        m_error = QStringLiteral("Файл правил имеет неверный формат JSON: %1").arg(path);
        return;
    }

    const QJsonObject root = document.object();
    m_featSlots = root.value(QStringLiteral("featSlots")).toObject();
    m_spellcasting = root.value(QStringLiteral("spellcasting")).toObject();

    if (m_featSlots.isEmpty() || m_spellcasting.isEmpty()) {
        m_error = QStringLiteral("Файл правил не содержит обязательных секций featSlots/spellcasting: %1").arg(path);
    }
}

QString CharacterProgressionRules::loadError() const
{
    ensureLoaded();
    return m_error;
}

int CharacterProgressionRules::tableValue(const QJsonValue &value, int classLevel) const
{
    const QJsonArray table = value.toArray();
    if (table.isEmpty() || classLevel <= 0) {
        return 0;
    }

    const int index = qBound(0, classLevel - 1, table.size() - 1);
    return table.at(index).toInt();
}

int CharacterProgressionRules::evaluateFormula(const QJsonObject &ruleObject, const QString &fieldName, const Character *character, int classLevel, const QString &abilityName) const
{
    const QString formula = ruleObject.value(fieldName).toString();
    if (formula.isEmpty()) {
        return 0;
    }

    const int minimum = ruleObject.value(
        fieldName == QStringLiteral("preparedFormula")
            ? QStringLiteral("preparedMinimum")
            : QStringLiteral("leveledMinimum")).toInt(0);
    const int modifier = Character::abilityModifier(characterAbilityScore(character, abilityName));

    int value = 0;
    if (formula == QStringLiteral("level_plus_mod")) {
        value = classLevel + modifier;
    } else if (formula == QStringLiteral("half_level_plus_mod")) {
        value = classLevel / 2 + modifier;
    }

    return qMax(minimum, value);
}

int CharacterProgressionRules::featSlotsForClass(const QString &className, int classLevel) const
{
    ensureLoaded();
    if (!m_error.isEmpty() || classLevel <= 0) {
        return 0;
    }

    QJsonArray thresholds = m_featSlots.value(className).toArray();
    if (thresholds.isEmpty()) {
        thresholds = m_featSlots.value(QStringLiteral("default")).toArray();
    }

    int slotCount = 0;
    for (const QJsonValue &threshold : thresholds) {
        if (classLevel >= threshold.toInt()) {
            ++slotCount;
        }
    }
    return slotCount;
}

SpellSelectionRule CharacterProgressionRules::spellSelectionRuleForClass(const Character *character, const QString &className, int classLevel) const
{
    ensureLoaded();

    SpellSelectionRule rule;
    rule.className = className;
    rule.classLevel = classLevel;

    if (!character || !m_error.isEmpty() || classLevel <= 0) {
        return rule;
    }

    const QJsonObject ruleObject = m_spellcasting.value(className).toObject();
    if (ruleObject.isEmpty()) {
        return rule;
    }

    rule.mode = ruleObject.value(QStringLiteral("mode")).toString();
    rule.abilityName = ruleObject.value(QStringLiteral("ability")).toString();
    rule.cantripLimit = tableValue(ruleObject.value(QStringLiteral("cantrips")), classLevel);
    rule.maxSpellLevel = tableValue(ruleObject.value(QStringLiteral("maxSpellLevel")), classLevel);
    rule.usesSpellbook = rule.mode == QStringLiteral("spellbook_prepared");

    if (rule.usesSpellbook) {
        rule.leveledLimit = tableValue(ruleObject.value(QStringLiteral("spellbook")), classLevel);
    } else if (ruleObject.contains(QStringLiteral("leveled"))) {
        rule.leveledLimit = tableValue(ruleObject.value(QStringLiteral("leveled")), classLevel);
    } else {
        rule.leveledLimit = evaluateFormula(ruleObject, QStringLiteral("leveledFormula"), character, classLevel, rule.abilityName);
    }

    if (ruleObject.contains(QStringLiteral("preparedFormula"))) {
        rule.preparedLimit = evaluateFormula(ruleObject, QStringLiteral("preparedFormula"), character, classLevel, rule.abilityName);
    }

    rule.exactLeveledLimit = ruleObject.value(QStringLiteral("exactLeveledLimit")).toBool(ruleObject.contains(QStringLiteral("leveled")) || rule.usesSpellbook);
    rule.exactPreparedLimit = ruleObject.value(QStringLiteral("exactPreparedLimit")).toBool(false);
    rule.leveledLabel = ruleObject.value(QStringLiteral("leveledLabel")).toString(defaultLabelForMode(rule.mode));
    rule.preparedLabel = ruleObject.value(QStringLiteral("preparedLabel")).toString(QStringLiteral("подготовленных заклинаний"));
    rule.note = ruleObject.value(QStringLiteral("note")).toString();

    const QJsonObject perLevelCaps = ruleObject.value(QStringLiteral("perSpellLevelCaps")).toObject();
    for (auto it = perLevelCaps.begin(); it != perLevelCaps.end(); ++it) {
        const int spellLevel = it.key().toInt();
        const int cap = tableValue(it.value(), classLevel);
        if (spellLevel > 0 && cap > 0) {
            rule.perSpellLevelCaps.insert(spellLevel, cap);
        }
    }

    return rule;
}

bool CharacterProgressionRules::characterCanUseSpellcasting(const Character *character, const QMap<QString, int> &classLevels) const
{
    ensureLoaded();
    if (!character || !m_error.isEmpty()) {
        return false;
    }

    for (auto it = classLevels.begin(); it != classLevels.end(); ++it) {
        if (spellSelectionRuleForClass(character, it.key(), it.value()).isValid()) {
            return true;
        }
    }
    return false;
}
