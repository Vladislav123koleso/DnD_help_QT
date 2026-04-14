#ifndef CHARACTER_H
#define CHARACTER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QStringList>
#include "spell.h"
#include "item.h"

class Character : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString race READ race WRITE setRace NOTIFY raceChanged)
    Q_PROPERTY(QString characterClass READ characterClass WRITE setCharacterClass NOTIFY characterClassChanged)

public:
    explicit Character(QObject *parent = nullptr);
    static int abilityModifier(int score);
    static int clampAbilityScore(int score);

    QString name() const;
    void setName(const QString &name);

    QString characterClass() const;
    void setCharacterClass(const QString &characterClass);

    QString race() const;
    void setRace(const QString &race);

    int level;
    int strength;
    int dexterity;
    int constitution;
    int intelligence;
    int wisdom;
    int charisma;

    // Combat Stats
    int maxHp;
    int currentHp;
    int tempHp;
    int armorClass;
    int initiative;
    int hitDie;
    int proficiencyBonus;
    QString alignment;

    // Description & RP
    QString background;
    QString appearance;
    QString age;
    QString height;
    QString weight;
    QString skin;
    QString hair;

    // Derived or specific stats
    int speed;
    int flyingSpeed;
    QString size;
    QStringList languages;
    QMap<QString, QString> traits; // Features/Traits (Name -> Desc)
    
    // Combat
    QStringList attacks; // Simple list for now: "Claws: 1d4 slashing"

    QList<Spell> spells;
    QList<Item> inventory;

signals:
    void nameChanged();
    void characterClassChanged();
    void raceChanged();

private:
    QString m_name;
    QString m_characterClass;
    QString m_race;
};

#endif // CHARACTER_H
