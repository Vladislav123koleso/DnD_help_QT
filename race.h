#ifndef RACE_H
#define RACE_H

#include <QString>
#include <QMap>
#include <QStringList>

class Race {
public:
    Race();
    QString name;
    QString description;
    
    // Ability Score Increases
    QMap<QString, int> abilityScoreIncrease;
    
    // Physical attributes
    QString size;
    int speed;
    int flyingSpeed;
    
    // Other features
    QMap<QString, QString> traits; // Name -> Description
    QStringList languages;
    QStringList subraces;
    
    // Visuals
    QString imagePath;
};

#endif // RACE_H
