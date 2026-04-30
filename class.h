#ifndef CLASS_H
#define CLASS_H

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

struct ClassSection {
public:
    QString title;
    QString description;
    QString levelText;
    int levelRequirement = 0;
    bool optional = false;

    QJsonObject toJson() const;
    static ClassSection fromJson(const QJsonObject &object);
};

struct ClassLevelProgression {
public:
    int level = 0;
    int proficiencyBonus = 0;
    QStringList features;

    QJsonObject toJson() const;
    static ClassLevelProgression fromJson(const QJsonObject &object);
};

struct ClassSubclass {
public:
    QString name;
    QString description;
    QList<ClassSection> sections;

    QJsonObject toJson() const;
    static ClassSubclass fromJson(const QJsonObject &object);
};

class Class {
public:
    Class();
    static Class fromJson(const QJsonObject &object);
    QJsonObject toJson() const;
    QJsonObject extraDataToJson() const;
    void loadExtraData(const QJsonObject &object);

    QString slug;
    QString name;
    QString source;
    QString description;
    
    int hitDie = 0;
    QString primaryAbility;
    QStringList savingThrowProficiencies;
    QStringList armorProficiencies;
    QStringList weaponProficiencies;
    QStringList toolProficiencies;
    QStringList skillChoices;
    int skillChoiceCount = 0;
    QStringList startingEquipment;
    QString multiclassRequirement;
    QStringList multiclassProficiencies;
    QString multiclassSpellcastingNote;
    QList<ClassLevelProgression> progression;
    QList<ClassSection> featureSections;
    QList<ClassSubclass> subclasses;
    
    // Visuals
    QString imagePath;
};

#endif // CLASS_H
