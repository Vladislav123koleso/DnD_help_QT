#include "databasemanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager instance;
    return instance;
}

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent)
{
}

bool DatabaseManager::addCreature(const Creature& creature)
{
    QSqlQuery query;
    query.prepare("INSERT INTO creatures (name, name_eng, type, source, ac, hp, speed, "
                  "str, dex, con, inte, wis, cha, senses, languages, challenge, "
                  "traits, actions, legendary_actions, description) "
                  "VALUES (:name, :name_eng, :type, :source, :ac, :hp, :speed, "
                  ":str, :dex, :con, :inte, :wis, :cha, :senses, :languages, :challenge, "
                  ":traits, :actions, :legendary_actions, :description)");

    query.bindValue(":name", creature.name);
    query.bindValue(":name_eng", creature.nameEng);
    query.bindValue(":type", creature.type);
    query.bindValue(":source", creature.source);
    query.bindValue(":ac", creature.ac);
    query.bindValue(":hp", creature.hp);
    query.bindValue(":speed", creature.speed);
    
    query.bindValue(":str", creature.str);
    query.bindValue(":dex", creature.dex);
    query.bindValue(":con", creature.con);
    query.bindValue(":inte", creature.inte);
    query.bindValue(":wis", creature.wis);
    query.bindValue(":cha", creature.cha);
    
    query.bindValue(":senses", creature.senses);
    query.bindValue(":languages", creature.languages);
    query.bindValue(":challenge", creature.challenge);
    
    query.bindValue(":traits", Creature::actionsToJson(creature.traits));
    query.bindValue(":actions", Creature::actionsToJson(creature.actions));
    query.bindValue(":legendary_actions", Creature::actionsToJson(creature.legendaryActions));
    query.bindValue(":description", creature.description);

    if (!query.exec()) {
        qDebug() << "Error adding creature:" << query.lastError().text();
        return false;
    }
    return true;
}

QList<Creature> DatabaseManager::getAllCreatures()
{
    QList<Creature> list;
    QSqlQuery query("SELECT * FROM creatures");
    while (query.next()) {
        Creature c;
        c.id = query.value("id").toInt();
        c.name = query.value("name").toString();
        c.nameEng = query.value("name_eng").toString();
        c.type = query.value("type").toString();
        c.source = query.value("source").toString();
        c.ac = query.value("ac").toString();
        c.hp = query.value("hp").toString();
        c.speed = query.value("speed").toString();
        
        c.str = query.value("str").toInt();
        c.dex = query.value("dex").toInt();
        c.con = query.value("con").toInt();
        c.inte = query.value("inte").toInt();
        c.wis = query.value("wis").toInt();
        c.cha = query.value("cha").toInt();
        
        c.senses = query.value("senses").toString();
        c.languages = query.value("languages").toString();
        c.challenge = query.value("challenge").toString();
        
        c.traits = Creature::parseActions(query.value("traits").toString());
        c.actions = Creature::parseActions(query.value("actions").toString());
        c.legendaryActions = Creature::parseActions(query.value("legendary_actions").toString());
        
        c.description = query.value("description").toString();
        
        list.append(c);
    }
    return list;
}

// ==================== Races ====================

bool DatabaseManager::addRace(const Race& race)
{
    QSqlQuery query;
    query.prepare("INSERT INTO races (name, description, speed, size, "
                  "asi_str, asi_dex, asi_con, asi_int, asi_wis, asi_cha, traits, languages) "
                  "VALUES (:name, :desc, :speed, :size, :s, :d, :co, :i, :w, :ch, :traits, :langs)");
    
    query.bindValue(":name", race.name);
    query.bindValue(":desc", race.description);
    query.bindValue(":speed", race.speed);
    query.bindValue(":size", race.size);
    
    query.bindValue(":s", race.abilityScoreIncrease.value("Strength", 0));
    query.bindValue(":d", race.abilityScoreIncrease.value("Dexterity", 0));
    query.bindValue(":co", race.abilityScoreIncrease.value("Constitution", 0));
    query.bindValue(":i", race.abilityScoreIncrease.value("Intelligence", 0));
    query.bindValue(":w", race.abilityScoreIncrease.value("Wisdom", 0));
    query.bindValue(":ch", race.abilityScoreIncrease.value("Charisma", 0));
    
    // Serialize traits map to JSON string
    QJsonObject traitsObj;
    for(auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        traitsObj.insert(it.key(), it.value());
    }
    query.bindValue(":traits", QJsonDocument(traitsObj).toJson(QJsonDocument::Compact));
    
    // Serialize languages list to JSON string
    QJsonArray langArr = QJsonArray::fromStringList(race.languages);
    query.bindValue(":langs", QJsonDocument(langArr).toJson(QJsonDocument::Compact));
    
    if (!query.exec()) {
        qDebug() << "Error adding race:" << query.lastError().text();
        return false;
    }
    return true;
}

void DatabaseManager::importRacesFromJson(const QString& filePath)
{
    QString resolvedPath = filePath;
    if (!QFile::exists(resolvedPath)) {
        QDir dir(QCoreApplication::applicationDirPath());
        if (dir.cdUp() && dir.cdUp() && dir.cdUp()) {
            QString candidate = dir.filePath(filePath);
            if (QFile::exists(candidate)) {
                resolvedPath = candidate;
            }
        }
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open Races JSON:" << resolvedPath;
        return;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;
    
    QSqlDatabase::database().transaction();
    QSqlQuery("DELETE FROM races").exec();
    
    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        Race r;
        r.name = obj["name"].toString();
        r.description = obj["description"].toString();
        r.speed = obj["speed"].toInt();
        r.size = obj["size"].toString();
        
        QJsonObject asi = obj["asi"].toObject();
        if (asi.contains("str")) r.abilityScoreIncrease["Strength"] = asi["str"].toInt();
        if (asi.contains("dex")) r.abilityScoreIncrease["Dexterity"] = asi["dex"].toInt();
        if (asi.contains("con")) r.abilityScoreIncrease["Constitution"] = asi["con"].toInt();
        if (asi.contains("int")) r.abilityScoreIncrease["Intelligence"] = asi["int"].toInt();
        if (asi.contains("wis")) r.abilityScoreIncrease["Wisdom"] = asi["wis"].toInt();
        if (asi.contains("cha")) r.abilityScoreIncrease["Charisma"] = asi["cha"].toInt();
        
        QJsonObject traits = obj["traits"].toObject();
        for(auto it = traits.begin(); it != traits.end(); ++it) {
             r.traits.insert(it.key(), it.value().toString());
        }
        
        QJsonArray langs = obj["languages"].toArray();
        for(const QJsonValue &l : langs) r.languages.append(l.toString());
        
        addRace(r);
    }
    
    QSqlDatabase::database().commit();
    qDebug() << "Races imported from" << resolvedPath;
}

QList<Race> DatabaseManager::getAllRaces()
{
    QList<Race> list;
    QSqlQuery query("SELECT * FROM races");
    while (query.next()) {
        Race r;
        r.name = query.value("name").toString();
        r.description = query.value("description").toString();
        r.speed = query.value("speed").toInt();
        r.size = query.value("size").toString();
        
        r.abilityScoreIncrease["Strength"] = query.value("asi_str").toInt();
        r.abilityScoreIncrease["Dexterity"] = query.value("asi_dex").toInt();
        r.abilityScoreIncrease["Constitution"] = query.value("asi_con").toInt();
        r.abilityScoreIncrease["Intelligence"] = query.value("asi_int").toInt();
        r.abilityScoreIncrease["Wisdom"] = query.value("asi_wis").toInt();
        r.abilityScoreIncrease["Charisma"] = query.value("asi_cha").toInt();
        
        QString traitsJson = query.value("traits").toString();
        QJsonObject traitsObj = QJsonDocument::fromJson(traitsJson.toUtf8()).object();
        for(auto it = traitsObj.begin(); it != traitsObj.end(); ++it) {
            r.traits.insert(it.key(), it.value().toString());
        }
        
        QString langsJson = query.value("languages").toString();
        QJsonArray langsArr = QJsonDocument::fromJson(langsJson.toUtf8()).array();
        for(const QJsonValue &v : langsArr) r.languages.append(v.toString());
        
        list.append(r);
    }
    return list;
}

// ==================== Classes ====================

bool DatabaseManager::addClass(const Class& cls)
{
    QSqlQuery query;
    query.prepare("INSERT INTO classes (name, description, hit_die, primary_ability, "
                  "saving_throws, armor_prof, weapon_prof, features) "
                  "VALUES (:name, :desc, :hd, :pa, :st, :ap, :wp, :feat)");
                  
    query.bindValue(":name", cls.name);
    query.bindValue(":desc", cls.description);
    query.bindValue(":hd", cls.hitDie);
    query.bindValue(":pa", cls.primaryAbility);
    
    query.bindValue(":st", QJsonDocument(QJsonArray::fromStringList(cls.savingThrowProficiencies)).toJson(QJsonDocument::Compact));
    query.bindValue(":ap", QJsonDocument(QJsonArray::fromStringList(cls.armorProficiencies)).toJson(QJsonDocument::Compact));
    query.bindValue(":wp", QJsonDocument(QJsonArray::fromStringList(cls.weaponProficiencies)).toJson(QJsonDocument::Compact));
     // Features not fully implemented in Class struct yet, storing empty JSON for now
    query.bindValue(":feat", "{}"); 
    
    if (!query.exec()) {
        qDebug() << "Error adding class:" << query.lastError().text();
        return false;
    }
    return true;
}

void DatabaseManager::importClassesFromJson(const QString& filePath)
{
     QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open Classes JSON:" << filePath;
        return;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;
    
    QSqlDatabase::database().transaction();
    QSqlQuery("DELETE FROM classes").exec();
    
    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        Class c;
        c.name = obj["name"].toString();
        c.description = obj["description"].toString();
        c.hitDie = obj["hitDie"].toInt();
        c.primaryAbility = obj["primaryAbility"].toString();
        
        QJsonArray saves = obj["savingThrowProficiencies"].toArray();
        for(const QJsonValue &v : saves) c.savingThrowProficiencies.append(v.toString());
        
        QJsonArray armor = obj["armorProficiencies"].toArray();
        for(const QJsonValue &v : armor) c.armorProficiencies.append(v.toString());
        
        QJsonArray wpns = obj["weaponProficiencies"].toArray();
        for(const QJsonValue &v : wpns) c.weaponProficiencies.append(v.toString());
        
        addClass(c);
    }
    
    QSqlDatabase::database().commit();
    qDebug() << "Classes imported.";
}

QList<Class> DatabaseManager::getAllClasses()
{
    QList<Class> list;
    QSqlQuery query("SELECT * FROM classes");
    while (query.next()) {
        Class c;
        c.name = query.value("name").toString();
        c.description = query.value("description").toString();
        c.hitDie = query.value("hit_die").toInt();
        c.primaryAbility = query.value("primary_ability").toString();
        
        QJsonArray saves = QJsonDocument::fromJson(query.value("saving_throws").toString().toUtf8()).array();
        for(const QJsonValue &v : saves) c.savingThrowProficiencies.append(v.toString());
        
        QJsonArray armor = QJsonDocument::fromJson(query.value("armor_prof").toString().toUtf8()).array();
        for(const QJsonValue &v : armor) c.armorProficiencies.append(v.toString());
        
        QJsonArray wpns = QJsonDocument::fromJson(query.value("weapon_prof").toString().toUtf8()).array();
        for(const QJsonValue &v : wpns) c.weaponProficiencies.append(v.toString());
        
        list.append(c);
    }
    return list;
}

void DatabaseManager::importBestiaryFromJson(const QString& filePath)
{
    QString resolvedPath = filePath;
    
    // 1. Check if file exists as provided
    if (!QFile::exists(resolvedPath)) {
        // 2. Try to find in project root (useful for Debugging from Build folder)
        QDir dir(QCoreApplication::applicationDirPath());
        // Go up from debug/release bin folder to project root
        // build/Desktop.../Debug/debug/app.exe -> 3 levels up usually
        // But checking iteratively is safer or just assuming 3 levels for standard Qt Creator setup
        if (dir.cdUp() && dir.cdUp() && dir.cdUp()) {
            QString candidate = dir.filePath(filePath);
            if (QFile::exists(candidate)) {
                resolvedPath = candidate;
                qDebug() << "Resolved Bestiary JSON path:" << resolvedPath;
            } else {
                 // Try one more level explicitly if needed (shadow builds can be deep)
                 // or different folder structure
                 qDebug() << "Candidate path not found:" << candidate;
            }
        }
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open JSON file for import:" << resolvedPath << "(Original:" << filePath << ")";
        qDebug() << "Current Working Dir:" << QDir::currentPath();
        qDebug() << "App Dir:" << QCoreApplication::applicationDirPath();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        qDebug() << "JSON is not an array";
        return;
    }

    QSqlDatabase::database().transaction();

    // Clear existing table for Update
    QSqlQuery deleteQuery;
    deleteQuery.exec("DELETE FROM creatures");

    QJsonArray array = doc.array();
    qDebug() << "Importing" << array.size() << "creatures...";

    for (const QJsonValue &value : array) {
        if (!value.isObject()) continue;
        QJsonObject obj = value.toObject();

        Creature c;
        c.name = obj["name"].toString();
        c.nameEng = obj["name_eng"].toString();
        c.type = obj["type"].toString();
        c.source = obj["source"].toString();
        c.ac = obj["ac"].toString();
        c.hp = obj["hp"].toString();
        c.speed = obj["speed"].toString();
        
        QJsonObject statsObj = obj["stats"].toObject();
        c.str = statsObj["str"].toInt();
        c.dex = statsObj["dex"].toInt();
        c.con = statsObj["con"].toInt();
        c.inte = statsObj["int"].toInt();
        c.wis = statsObj["wis"].toInt();
        c.cha = statsObj["cha"].toInt();
        
        c.senses = obj["senses"].toString();
        c.languages = obj["languages"].toString();
        c.challenge = obj["challenge"].toString();
        c.description = obj["description"].toString();
        
        // Traits
        if (obj["traits"].isArray()) {
            QJsonArray tArr = obj["traits"].toArray();
            for (const auto& tVal : tArr) {
                QJsonObject tObj = tVal.toObject();
                c.traits.append({tObj["name"].toString(), tObj["text"].toString()});
            }
        }
        
        // Actions
        if (obj["actions"].isArray()) {
            QJsonArray aArr = obj["actions"].toArray();
            for (const auto& aVal : aArr) {
                QJsonObject aObj = aVal.toObject();
                c.actions.append({aObj["name"].toString(), aObj["text"].toString()});
            }
        }
        
        // Legendary
        if (obj["legendary_actions"].isArray()) {
            QJsonArray lArr = obj["legendary_actions"].toArray();
            for (const auto& lVal : lArr) {
                QJsonObject lObj = lVal.toObject();
                c.legendaryActions.append({lObj["name"].toString(), lObj["text"].toString()});
            }
        }

        addCreature(c);
    }
    QSqlDatabase::database().commit();
    qDebug() << "Bestiary Import complete.";
}

DatabaseManager::~DatabaseManager()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::connectToDatabase()
{
    // Используем SQLite
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    
    // 1. Try looking in the current working directory
    QString dbName = "dnd_rules.db";
    QString dbPath = dbName;

#ifdef QT_DEBUG
    // В режиме отладки пытаемся найти БД в корне проекта, чтобы не копировать её каждый раз
    // Структура: build/Kit/Debug/app.exe -> ../../../dnd_rules.db
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.cdUp() && dir.cdUp() && dir.cdUp()) {
         QString devDbPath = dir.filePath(dbName);
         if (QFile::exists(devDbPath)) {
             dbPath = devDbPath;
             qDebug() << "Debug Mode: Using database from source root:" << dbPath;
         }
    }
#endif
    
    // 2. If not found (and not overridden by debug logic), try looking next to the executable
    if (!QFile::exists(dbPath)) {
        QString appDir = QCoreApplication::applicationDirPath();
        QString dbPathInAppDir = QDir(appDir).filePath(dbName);
        if (QFile::exists(dbPathInAppDir)) {
            dbPath = dbPathInAppDir;
        }
    }

    qDebug() << "Connecting to database at:" << QFileInfo(dbPath).absoluteFilePath();

    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qDebug() << "Error: connection with database failed";
        qDebug() << m_db.lastError().text();
        return false;
    } else {
        qDebug() << "Database: connection ok";
    }
    
    return initTables();
}

bool DatabaseManager::initTables()
{
    QSqlQuery query;
    
    // Таблица Заклинаний
    // Обновленная структура: добавлены ritual, classes
    QString createSpellsTable = R"(
        CREATE TABLE IF NOT EXISTS spells (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            level INTEGER,
            school TEXT,
            ritual BOOLEAN,
            casting_time TEXT,
            range TEXT,
            components TEXT,
            duration TEXT,
            description TEXT,
            classes TEXT,
            subclasses TEXT,
            source TEXT,
            upper TEXT
        )
    )";

    if (!query.exec(createSpellsTable)) {
        qDebug() << "Could not create spells table:" << query.lastError();
        return false;
    }

    // Таблица Предметов
    QString createItemsTable = R"(
        CREATE TABLE IF NOT EXISTS items (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            name_eng TEXT,
            type TEXT,
            rarity TEXT,
            cost TEXT,
            weight TEXT,
            source TEXT,
            description TEXT
        )
    )";

    if (!query.exec(createItemsTable)) {
        qDebug() << "Could not create items table:" << query.lastError();
        return false;
    }

    // Таблица Бестиария
    QString createCreaturesTable = R"(
        CREATE TABLE IF NOT EXISTS creatures (
             id INTEGER PRIMARY KEY AUTOINCREMENT,
             name TEXT,
             name_eng TEXT,
             type TEXT,
             source TEXT,
             ac TEXT,
             hp TEXT,
             speed TEXT, 
             str INTEGER, dex INTEGER, con INTEGER, inte INTEGER, wis INTEGER, cha INTEGER,
             senses TEXT,
             languages TEXT,
             challenge TEXT,
             traits TEXT, 
             actions TEXT, 
             legendary_actions TEXT,
             description TEXT
        )
    )";

    if (!query.exec(createCreaturesTable)) {
        qDebug() << "Could not create creatures table:" << query.lastError();
        return false;
    }

    // Таблица Рас
    QString createRacesTable = R"(
        CREATE TABLE IF NOT EXISTS races (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE,
            description TEXT,
            speed INTEGER,
            size TEXT,
            asi_str INTEGER DEFAULT 0,
            asi_dex INTEGER DEFAULT 0,
            asi_con INTEGER DEFAULT 0,
            asi_int INTEGER DEFAULT 0,
            asi_wis INTEGER DEFAULT 0,
            asi_cha INTEGER DEFAULT 0,
            traits TEXT,
            languages TEXT
        )
    )";

    if (!query.exec(createRacesTable)) {
        qDebug() << "Could not create races table:" << query.lastError();
        return false;
    }

    // Таблица Классов
    QString createClassesTable = R"(
        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE,
            description TEXT,
            hit_die INTEGER,
            primary_ability TEXT,
            saving_throws TEXT,
            armor_prof TEXT,
            weapon_prof TEXT,
            features TEXT
        )
    )";

    if (!query.exec(createClassesTable)) {
        qDebug() << "Could not create classes table:" << query.lastError();
        return false;
    }

    return true;
}

bool DatabaseManager::addSpell(const Spell& spell)
{
    QSqlQuery query;
    query.prepare("INSERT INTO spells (name, level, school, ritual, casting_time, range, components, duration, description, classes, subclasses, source, upper) "
                  "VALUES (:name, :level, :school, :ritual, :casting_time, :range, :components, :duration, :description, :classes, :subclasses, :source, :upper)");
    
    query.bindValue(":name", spell.name);
    query.bindValue(":level", spell.level);
    query.bindValue(":school", spell.school);
    query.bindValue(":ritual", spell.ritual);
    query.bindValue(":casting_time", spell.castingTime);
    query.bindValue(":range", spell.range);
    query.bindValue(":components", spell.components);
    query.bindValue(":duration", spell.duration);
    query.bindValue(":description", spell.description);
    query.bindValue(":classes", spell.classes);
    query.bindValue(":subclasses", spell.subclasses);
    query.bindValue(":source", spell.source);
    query.bindValue(":upper", spell.upper);
    
    if (!query.exec()) {
        qDebug() << "Error adding spell:" << query.lastError();
        return false;
    }
    return true;
}

QList<Spell> DatabaseManager::getAllSpells()
{
    return searchSpells("");
}

QList<Spell> DatabaseManager::searchSpells(const QString& searchText)
{
    QList<Spell> list;
    QSqlQuery query;
    
    if (searchText.isEmpty()) {
        query.prepare("SELECT * FROM spells");
    } else {
        query.prepare("SELECT * FROM spells WHERE name LIKE :search");
        query.bindValue(":search", "%" + searchText + "%");
    }
    
    if (query.exec()) {
        while (query.next()) {
            Spell s;
            s.name = query.value("name").toString();
            s.level = query.value("level").toInt();
            s.school = query.value("school").toString();
            s.ritual = query.value("ritual").toBool();
            s.castingTime = query.value("casting_time").toString();
            s.range = query.value("range").toString();
            s.components = query.value("components").toString();
            s.duration = query.value("duration").toString();
            s.description = query.value("description").toString();
            s.classes = query.value("classes").toString();
            s.subclasses = query.value("subclasses").toString();
            s.source = query.value("source").toString();
            s.upper = query.value("upper").toString();
            list.append(s);
        }
    } else {
        qDebug() << "Search error:" << query.lastError();
    }
    return list;
}

// ---------------------------------------------------------
// ITEMS IMPLEMENTATION
// ---------------------------------------------------------

bool DatabaseManager::addItem(const Item& item)
{
    QSqlQuery query;
    query.prepare("INSERT INTO items (name, name_eng, type, rarity, cost, weight, source, description) "
                  "VALUES (:name, :name_eng, :type, :rarity, :cost, :weight, :source, :description)");
    
    query.bindValue(":name", item.name);
    query.bindValue(":name_eng", item.nameEng);
    query.bindValue(":type", item.type);
    query.bindValue(":rarity", item.rarity);
    query.bindValue(":cost", item.cost);
    query.bindValue(":weight", item.weight);
    query.bindValue(":source", item.source);
    query.bindValue(":description", item.description);
    
    if (!query.exec()) {
        qDebug() << "Error adding item:" << query.lastError();
        return false;
    }
    return true;
}

QList<Item> DatabaseManager::getAllItems()
{
    return searchItems("");
}

QList<Item> DatabaseManager::searchItems(const QString& searchText)
{
    QList<Item> list;
    QSqlQuery query;
    
    if (searchText.isEmpty()) {
        query.prepare("SELECT * FROM items");
    } else {
        query.prepare("SELECT * FROM items WHERE name LIKE :search");
        query.bindValue(":search", "%" + searchText + "%");
    }
    
    if (query.exec()) {
        while (query.next()) {
            Item i;
            i.id = query.value("id").toInt();
            i.name = query.value("name").toString();
            i.nameEng = query.value("name_eng").toString();
            i.type = query.value("type").toString();
            i.rarity = query.value("rarity").toString();
            i.cost = query.value("cost").toString();
            i.weight = query.value("weight").toString();
            i.source = query.value("source").toString();
            i.description = query.value("description").toString();
            list.append(i);
        }
    } else {
        qDebug() << "Search item error:" << query.lastError();
    }
    return list;
}

void DatabaseManager::importItemsFromJson(const QString& filePath)
{
    QString resolvedPath = filePath;
    // 1. Check if file exists as provided
    if (!QFile::exists(resolvedPath)) {
        QDir dir(QCoreApplication::applicationDirPath());
        if (dir.cdUp() && dir.cdUp() && dir.cdUp()) {
            QString candidate = dir.filePath(filePath);
            if (QFile::exists(candidate)) {
                resolvedPath = candidate;
                qDebug() << "Resolved Items JSON path:" << resolvedPath;
            }
        }
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Could not open JSON file for reading:" << resolvedPath << "(Original:" << filePath << ")";
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isArray()) {
        qDebug() << "JSON is not an array";
        return;
    }

    QJsonArray array = doc.array();
    qDebug() << "Importing" << array.size() << "items from JSON...";

    m_db.transaction(); // Ускоряем вставку

    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();
        Item i;
        i.name = obj["name"].toString();
        i.nameEng = obj["name_eng"].toString();
        i.type = obj["type"].toString();
        i.rarity = obj["rarity"].toString();
        i.cost = obj["cost"].toString();
        i.weight = obj["weight"].toString();
        i.source = obj["source"].toString();
        i.description = obj["description"].toString();
        
        addItem(i);
    }

    m_db.commit();
    qDebug() << "Items import finished.";
}

void DatabaseManager::populateInitialData()
{
    // 1. Spells check
    QSqlQuery querySpells("SELECT COUNT(*) FROM spells");
    if (!querySpells.next() || querySpells.value(0).toInt() == 0) {
        // Добавляем пару тестовых заклинаний
        Spell fireball;
        fireball.name = "Fireball";
        fireball.level = 3;
        fireball.school = "Evocation";
        fireball.description = "A bright streak ...";
        addSpell(fireball);
    }
    
    // 2. Items check
    QSqlQuery queryItems("SELECT COUNT(*) FROM items");
    int itemCount = 0;
    if (queryItems.next()) {
        itemCount = queryItems.value(0).toInt();
    }

    // Всегда проверяем наличие JSON для обновления данных (Development Mode: Force Update)
    QStringList paths = {
        "scripts/final_item.json",
        "../scripts/final_item.json",
        "../../scripts/final_item.json",
        "../../../scripts/final_item.json",
        "D:/repos/qt/DnD_help/scripts/final_item.json"
    };
    
    QString jsonPath;
    for (const QString& path : paths) {
        if (QFile::exists(path)) {
            jsonPath = path;
            break;
        }
    }

    if (!jsonPath.isEmpty()) {
        // Если файл найден - перезаливаем базу (чтобы подхватить изменения парсера)
        qDebug() << "Found items JSON at" << jsonPath << "- Refreshing database...";
        
        QSqlQuery clearExec;
        if (clearExec.exec("DELETE FROM items")) {
             // Сброс автоинкремента (опционально)
             QSqlQuery("DELETE FROM sqlite_sequence WHERE name='items'");
             importItemsFromJson(jsonPath);
        } else {
             qDebug() << "Failed to clear items table:" << clearExec.lastError();
        }
    } else if (itemCount == 0) {
        qDebug() << "No items in DB and no JSON found.";
    }

    // 3. Bestiary check
    QSqlQuery queryCreatures("SELECT COUNT(*) FROM creatures");
    int creatureCount = 0;
    if (queryCreatures.next()) {
        creatureCount = queryCreatures.value(0).toInt();
    }

    QStringList bestiaryPaths = {
        "scripts/final_bestiary.json",
        "../scripts/final_bestiary.json",
        "../../scripts/final_bestiary.json",
        "../../../scripts/final_bestiary.json",
        "D:/repos/qt/DnD_help/scripts/final_bestiary.json"
    };

    QString bestiaryJsonPath;
    for (const QString& path : bestiaryPaths) {
        if (QFile::exists(path)) {
            bestiaryJsonPath = path;
            break;
        }
    }

    if (!bestiaryJsonPath.isEmpty()) {
        qDebug() << "Found bestiary JSON at" << bestiaryJsonPath << "- Refreshing database...";
        // importBestiaryFromJson очищает таблицу сама
        importBestiaryFromJson(bestiaryJsonPath);
    } else if (creatureCount == 0) {
        qDebug() << "No creatures in DB and no JSON found.";
    }

    qDebug() << "Initial data check finished.";
}
