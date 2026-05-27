#ifndef NPC_H
#define NPC_H

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QList>

#include "item.h"
#include "spell.h"

class Character;

struct NpcFolder {
    QString id;
    QString name;
    bool expanded = true;
    int sortOrder = 0;

    QJsonObject toJson() const;
    static NpcFolder fromJson(const QJsonObject &object);
};

struct NpcEntry {
    QString id;
    QString listName;
    QString folderId;
    int sortOrder = 0;

    QString name;
    QString race;
    QString characterClass;
    QString alignment;
    QString size = QStringLiteral("Средний");
    QString creatureType;
    QString challengeRating;
    int experienceReward = 0;
    int level = 1;

    int strength = 10;
    int dexterity = 10;
    int constitution = 10;
    int intelligence = 10;
    int wisdom = 10;
    int charisma = 10;

    int armorClass = 10;
    int maxHp = 10;
    int currentHp = 10;
    int speed = 30;
    int initiative = 0;
    int proficiencyBonus = 2;
    int passivePerception = 10;

    QString armorDescription;
    QString armorSource;
    QString shieldSource;
    QString hitDiceFormula;
    QString speedDescription;
    QString senses;
    QString damageVulnerabilities;
    QString damageResistances;
    QString damageImmunities;
    QString conditionImmunities;

    QString appearance;
    QString age;
    QString height;
    QString weight;
    QString skin;
    QString hair;

    QString description;
    QString notes;

    QStringList languages;
    QStringList skillProficiencies;
    QStringList savingThrowProficiencies;
    QStringList armorProficiencies;
    QStringList weaponProficiencies;
    QStringList attacks;
    QMap<QString, QString> traits;

    QList<Spell> spells;
    QList<Item> inventory;

    QJsonObject characterRulesJson;

    static NpcEntry createNew(const QString &listName);
    QJsonObject toJson() const;
    static NpcEntry fromJson(const QJsonObject &object);
};

struct NpcStorageData {
    QMap<QString, NpcFolder> folders;
    QMap<QString, NpcEntry> npcs;

    static NpcStorageData loadFromFile(const QString &path);
};

QString formatNpcSummary(const NpcEntry &entry);

void syncCharacterFromNpcEntry(const NpcEntry &entry, Character &character);
void syncNpcEntryFromCharacter(const Character &character, NpcEntry &entry);

#endif // NPC_H
