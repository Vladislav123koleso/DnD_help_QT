#include "character.h"
#include <QtGlobal>
#include <cmath>

Character::Character(QObject *parent) : QObject(parent),
      level(1), strength(10), dexterity(10), constitution(10),
      intelligence(10), wisdom(10), charisma(10),
      maxHp(0), currentHp(0), tempHp(0), armorClass(10),
      initiative(0), hitDie(8), proficiencyBonus(2),
      speed(30), flyingSpeed(0), size("Medium")
{

}

int Character::abilityModifier(int score)
{
    return static_cast<int>(std::floor((score - 10) / 2.0));
}

int Character::clampAbilityScore(int score)
{
    return qBound(1, score, 20);
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
