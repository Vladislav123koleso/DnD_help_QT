#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVariant>
#include "spell.h"
#include "item.h"
#include "creature.h"
#include "race.h"
#include "class.h"
#include "background.h"
#include "character.h"
#include "feat.h"

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager& instance();

    bool connectToDatabase();
    bool initTables();
    
    // Методы для работы с заклинаниями
    bool addSpell(const Spell& spell);
    QList<Spell> getAllSpells();
    QList<Spell> getSpellsByLevel(int level);
    QList<Spell> searchSpells(const QString& searchText);
    
    // Методы для работы с предметами
    bool addItem(const Item& item);
    QList<Item> getAllItems();
    QList<Item> searchItems(const QString& searchText);
    void importItemsFromJson(const QString& filePath);

    // Методы для работы с бестиарием
    bool addCreature(const Creature& creature);
    QList<Creature> getAllCreatures();
    void importBestiaryFromJson(const QString& filePath);

    // Методы для работы с расами
    bool addRace(const Race& race);
    QList<Race> getAllRaces();
    void importRacesFromJson(const QString& filePath);

    // Методы для работы с классами
    bool addClass(const Class& cls);
    QList<Class> getAllClasses();
    void importClassesFromJson(const QString& filePath);

    // Методы для работы с предысториями
    bool addBackground(const Background& background);
    QList<Background> getAllBackgrounds();
    void importBackgroundsFromJson(const QString& filePath);

    // Методы для работы с чертами
    bool addFeat(const Feat& feat);
    QList<Feat> getAllFeats();
    void importFeatsFromJson(const QString& filePath);

    // Методы для работы с персонажами
    bool saveCharacter(const QString& campaignName, const Character& character);
    bool loadCharacter(const QString& campaignName, Character *character);

    // Метод для заполнения тестовыми данными
    void populateInitialData();

private:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();
    
    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H
