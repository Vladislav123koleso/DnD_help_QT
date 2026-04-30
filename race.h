#ifndef RACE_H
#define RACE_H

#include <QString>
#include <QMap>
#include <QStringList>

class Race {
public:
    Race();
    QString type;
    bool hasType;
    QString slug;
    bool hasSlug;
    QString name;
    QString source;
    bool hasSource;
    QString description;
    bool hasDescription;
    
    // Ability Score Increases
    QMap<QString, int> abilityScoreIncrease;
    bool hasAbilityScoreIncrease;
    
    // Physical attributes
    QString size;
    bool hasSize;
    int speed;
    bool hasSpeed;
    int flyingSpeed;
    bool hasFlyingSpeed;
    
    // Other features
    QMap<QString, QString> traits; // Name -> Description
    bool hasTraits;
    QStringList languages;
    bool hasLanguages;
    QStringList subraces;
    
    // Visuals
    QString imagePath;
};

#endif // RACE_H
