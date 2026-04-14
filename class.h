#ifndef CLASS_H
#define CLASS_H

#include <QString>
#include <QStringList>

class Class {
public:
    Class();
    QString name;
    QString description;
    
    int hitDie;
    QString primaryAbility;
    QStringList savingThrowProficiencies;
    QStringList armorProficiencies;
    QStringList weaponProficiencies;
    
    // Visuals
    QString imagePath;
};

#endif // CLASS_H
