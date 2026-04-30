#ifndef CHARACTERPROGRESSIONRULES_H
#define CHARACTERPROGRESSIONRULES_H

#include <QMap>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

class Character;

struct SpellSelectionRule {
    QString className;
    QString mode;
    QString abilityName;
    int classLevel = 0;
    int cantripLimit = 0;
    int leveledLimit = 0;
    int preparedLimit = 0;
    int maxSpellLevel = 0;
    bool exactLeveledLimit = false;
    bool exactPreparedLimit = false;
    bool usesSpellbook = false;
    QString leveledLabel;
    QString preparedLabel;
    QString note;
    QMap<int, int> perSpellLevelCaps;

    bool isValid() const;
};

class CharacterProgressionRules
{
public:
    static CharacterProgressionRules &instance();

    int featSlotsForClass(const QString &className, int classLevel) const;
    SpellSelectionRule spellSelectionRuleForClass(const Character *character, const QString &className, int classLevel) const;
    bool characterCanUseSpellcasting(const Character *character, const QMap<QString, int> &classLevels) const;
    QString loadError() const;

private:
    CharacterProgressionRules();

    void ensureLoaded() const;
    QString resolveRulesPath() const;
    int tableValue(const QJsonValue &value, int classLevel) const;
    int evaluateFormula(const QJsonObject &ruleObject, const QString &fieldName, const Character *character, int classLevel, const QString &abilityName) const;
    int characterAbilityScore(const Character *character, const QString &abilityName) const;

    mutable bool m_loaded = false;
    mutable QString m_error;
    mutable QJsonObject m_featSlots;
    mutable QJsonObject m_spellcasting;
};

#endif // CHARACTERPROGRESSIONRULES_H