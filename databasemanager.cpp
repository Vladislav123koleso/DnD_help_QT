#include "databasemanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>

namespace {

QMap<QString, int> elfAbilityBonuses(int dexterity, int intelligence, int wisdom, int charisma, int strength)
{
    return {
        {QStringLiteral("Strength"), strength},
        {QStringLiteral("Dexterity"), dexterity},
        {QStringLiteral("Constitution"), 0},
        {QStringLiteral("Intelligence"), intelligence},
        {QStringLiteral("Wisdom"), wisdom},
        {QStringLiteral("Charisma"), charisma}
    };
}

QMap<QString, QString> baseElfTraits()
{
    return {
        {QStringLiteral("Увеличение характеристик"), QStringLiteral("Ловкость +2.")},
        {QStringLiteral("Тёмное зрение"), QStringLiteral("Вы видите в темноте на 60 футов как при тусклом освещении и различаете только оттенки серого.")},
        {QStringLiteral("Обострённые чувства"), QStringLiteral("Вы владеете навыком Восприятие.")},
        {QStringLiteral("Наследие фей"), QStringLiteral("Вы совершаете с преимуществом спасброски против состояния очарованный, и магия не может вас усыпить.")},
        {QStringLiteral("Транс"), QStringLiteral("Вместо сна вы медитируете 4 часа и получаете преимущества продолжительного отдыха.")},
        {QStringLiteral("Языки"), QStringLiteral("Вы говорите, читаете и пишете на Общем и Эльфийском.")}
    };
}

Race normalizedBaseElf(const Race &sourceRace)
{
    Race race = sourceRace;
    race.slug = QStringLiteral("race/79-elf");
    race.name = QStringLiteral("Эльф");
    race.source = QStringLiteral("Player's Handbook");
    race.abilityScoreIncrease = elfAbilityBonuses(2, 0, 0, 0, 0);
    race.speed = 30;
    race.flyingSpeed = 0;
    race.size = QStringLiteral("Средний");
    race.traits = baseElfTraits();
    race.languages = {QStringLiteral("Общий"), QStringLiteral("Эльфийский")};
    return race;
}

Race legacyElfVariant(
    const Race &sourceRace,
    const QString &slug,
    const QString &name,
    const QString &source,
    const QString &description,
    const QMap<QString, int> &asi,
    const QMap<QString, QString> &extraTraits,
    const QStringList &languages = {})
{
    Race race = normalizedBaseElf(sourceRace);
    race.slug = slug;
    race.name = name;
    race.source = source;
    race.description = description;
    race.abilityScoreIncrease = asi;
    for (auto it = extraTraits.begin(); it != extraTraits.end(); ++it) {
        race.traits.insert(it.key(), it.value());
    }
    if (!languages.isEmpty()) {
        race.languages = languages;
    }
    return race;
}

QList<Race> normalizedImportedRaces(const QList<Race> &races)
{
    QList<Race> normalized;
    QStringList raceNames;
    Race elfSource;
    bool hasElf = false;

    for (const Race &race : races) {
        if (race.name == QStringLiteral("Эльф")) {
            elfSource = race;
            hasElf = true;
            const Race cleanedElf = normalizedBaseElf(race);
            normalized.append(cleanedElf);
            raceNames << cleanedElf.name;
            continue;
        }

        normalized.append(race);
        raceNames << race.name;
    }

    if (!hasElf) {
        return normalized;
    }

    const auto appendIfMissing = [&](const Race &race) {
        if (!raceNames.contains(race.name)) {
            normalized.append(race);
            raceNames << race.name;
        }
    };

    appendIfMissing(legacyElfVariant(
        elfSource,
        QStringLiteral("race/79-high-elf"),
        QStringLiteral("Высший эльф"),
        QStringLiteral("Player's Handbook"),
        QStringLiteral("Высшие эльфы развивают магию и изящное владение традиционным оружием."),
        elfAbilityBonuses(2, 1, 0, 0, 0),
        {
            {QStringLiteral("Увеличение характеристик"), QStringLiteral("Ловкость +2, Интеллект +1.")},
            {QStringLiteral("Владение эльфийским оружием"), QStringLiteral("Вы владеете длинным мечом, коротким мечом, длинным луком и коротким луком.")},
            {QStringLiteral("Заговор"), QStringLiteral("Вы знаете один заговор из списка волшебника. Базовая характеристика для него — Интеллект.")},
            {QStringLiteral("Дополнительный язык"), QStringLiteral("Вы знаете ещё один язык по выбору.")}
        },
        {QStringLiteral("Общий"), QStringLiteral("Эльфийский"), QStringLiteral("Дополнительный язык по выбору")}));

    appendIfMissing(legacyElfVariant(
        elfSource,
        QStringLiteral("race/79-wood-elf"),
        QStringLiteral("Лесной эльф"),
        QStringLiteral("Player's Handbook"),
        QStringLiteral("Лесные эльфы быстрее других эльфов и лучше скрываются среди природных укрытий."),
        elfAbilityBonuses(2, 0, 1, 0, 0),
        {
            {QStringLiteral("Увеличение характеристик"), QStringLiteral("Ловкость +2, Мудрость +1.")},
            {QStringLiteral("Быстрые ноги"), QStringLiteral("Ваша скорость ходьбы увеличивается до 35 футов.")},
            {QStringLiteral("Маскировка в дикой местности"), QStringLiteral("Вы можете прятаться, если вас слабо скрывают листва, снег, дождь, туман и другие природные явления.")}
        }));

    appendIfMissing(legacyElfVariant(
        elfSource,
        QStringLiteral("race/79-drow"),
        QStringLiteral("Дроу"),
        QStringLiteral("Player's Handbook"),
        QStringLiteral("Дроу приспособлены к Подземью, владеют особой магией и страдают от яркого солнечного света."),
        elfAbilityBonuses(2, 0, 0, 1, 0),
        {
            {QStringLiteral("Увеличение характеристик"), QStringLiteral("Ловкость +2, Харизма +1.")},
            {QStringLiteral("Превосходное тёмное зрение"), QStringLiteral("Ваше тёмное зрение увеличивается до 120 футов.")},
            {QStringLiteral("Чувствительность к солнцу"), QStringLiteral("При прямом солнечном свете вы получаете помеху на броски атаки и проверки Восприятия, основанные на зрении.")},
            {QStringLiteral("Магия дроу"), QStringLiteral("С 3 уровня вы можете накладывать Огонь фей, а с 5 уровня — Тьму, восстанавливая использование после продолжительного отдыха.")},
            {QStringLiteral("Владение оружием дроу"), QStringLiteral("Вы владеете рапирой, коротким мечом и ручным арбалетом.")}
        }));

    appendIfMissing(legacyElfVariant(
        elfSource,
        QStringLiteral("race/79-grugach"),
        QStringLiteral("Гругач"),
        QStringLiteral("Mordenkainen's Tome of Foes"),
        QStringLiteral("Гругачи — дикие эльфы, полагающиеся на силу, оружейную выучку и простую магию природы."),
        elfAbilityBonuses(2, 0, 0, 0, 1),
        {
            {QStringLiteral("Увеличение характеристик"), QStringLiteral("Ловкость +2, Сила +1.")},
            {QStringLiteral("Боевая подготовка гругачей"), QStringLiteral("Вы владеете копьём, коротким луком, длинным луком и сетью.")},
            {QStringLiteral("Заговор"), QStringLiteral("Вы знаете один заговор из списка друида. Базовая характеристика для него — Мудрость.")}
        }));

    return normalized;
}

} // namespace

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
    query.prepare("INSERT INTO races (slug, name, source, description, speed, flying_speed, size, "
                  "asi_str, asi_dex, asi_con, asi_int, asi_wis, asi_cha, traits, languages) "
                  "VALUES (:slug, :name, :source, :desc, :speed, :flying_speed, :size, :s, :d, :co, :i, :w, :ch, :traits, :langs)");
    
    query.bindValue(":slug", race.slug);
    query.bindValue(":name", race.name);
    query.bindValue(":source", race.source);
    query.bindValue(":desc", race.description);
    query.bindValue(":speed", race.speed);
    query.bindValue(":flying_speed", race.flyingSpeed);
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
    
    QList<Race> importedRaces;
    const QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        Race r;
        r.slug = obj["slug"].toString();
        r.name = obj["name"].toString();
        r.source = obj["source"].toString();
        r.description = obj["description"].toString();
        r.speed = obj["speed"].toInt();
        r.flyingSpeed = obj["flyingSpeed"].toInt();
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
        
        importedRaces.append(r);
    }

    const QList<Race> normalizedRaces = normalizedImportedRaces(importedRaces);
    for (const Race &race : normalizedRaces) {
        addRace(race);
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
        r.slug = query.value("slug").toString();
        r.name = query.value("name").toString();
        r.source = query.value("source").toString();
        r.description = query.value("description").toString();
        r.speed = query.value("speed").toInt();
        r.flyingSpeed = query.value("flying_speed").toInt();
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
    query.prepare("INSERT INTO classes (slug, name, source, description, hit_die, primary_ability, "
                  "saving_throws, armor_prof, weapon_prof, features) "
                  "VALUES (:slug, :name, :source, :desc, :hd, :pa, :st, :ap, :wp, :feat)");
                  
    query.bindValue(":slug", cls.slug);
    query.bindValue(":name", cls.name);
    query.bindValue(":source", cls.source);
    query.bindValue(":desc", cls.description);
    query.bindValue(":hd", cls.hitDie);
    query.bindValue(":pa", cls.primaryAbility);
    
    query.bindValue(":st", QJsonDocument(QJsonArray::fromStringList(cls.savingThrowProficiencies)).toJson(QJsonDocument::Compact));
    query.bindValue(":ap", QJsonDocument(QJsonArray::fromStringList(cls.armorProficiencies)).toJson(QJsonDocument::Compact));
    query.bindValue(":wp", QJsonDocument(QJsonArray::fromStringList(cls.weaponProficiencies)).toJson(QJsonDocument::Compact));
    query.bindValue(":feat", QJsonDocument(cls.extraDataToJson()).toJson(QJsonDocument::Compact));
    
    if (!query.exec()) {
        qDebug() << "Error adding class:" << query.lastError().text();
        return false;
    }
    return true;
}

void DatabaseManager::importClassesFromJson(const QString& filePath)
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
        qDebug() << "Cannot open Classes JSON:" << resolvedPath;
        return;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;
    
    QSqlDatabase::database().transaction();
    QSqlQuery("DELETE FROM classes").exec();
    
    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        addClass(Class::fromJson(val.toObject()));
    }
    
    QSqlDatabase::database().commit();
    qDebug() << "Classes imported from" << resolvedPath;
}

QList<Class> DatabaseManager::getAllClasses()
{
    QList<Class> list;
    QSqlQuery query("SELECT * FROM classes");
    while (query.next()) {
        Class c;
        c.slug = query.value("slug").toString();
        c.name = query.value("name").toString();
        c.source = query.value("source").toString();
        c.description = query.value("description").toString();
        c.hitDie = query.value("hit_die").toInt();
        c.primaryAbility = query.value("primary_ability").toString();
        
        QJsonArray saves = QJsonDocument::fromJson(query.value("saving_throws").toString().toUtf8()).array();
        for(const QJsonValue &v : saves) c.savingThrowProficiencies.append(v.toString());
        
        QJsonArray armor = QJsonDocument::fromJson(query.value("armor_prof").toString().toUtf8()).array();
        for(const QJsonValue &v : armor) c.armorProficiencies.append(v.toString());
        
        QJsonArray wpns = QJsonDocument::fromJson(query.value("weapon_prof").toString().toUtf8()).array();
        for(const QJsonValue &v : wpns) c.weaponProficiencies.append(v.toString());

        const QJsonObject extra = QJsonDocument::fromJson(query.value("features").toString().toUtf8()).object();
        c.loadExtraData(extra);
        
        list.append(c);
    }
    return list;
}

bool DatabaseManager::addBackground(const Background& background)
{
    QSqlQuery query;
    query.prepare(
        "INSERT INTO backgrounds (slug, name, source, description, skill_prof, tool_prof, languages, equipment, feature_name, feature_description, traits) "
        "VALUES (:slug, :name, :source, :description, :skill_prof, :tool_prof, :languages, :equipment, :feature_name, :feature_description, :traits)");

    query.bindValue(":slug", background.slug);
    query.bindValue(":name", background.name);
    query.bindValue(":source", background.source);
    query.bindValue(":description", background.description);
    query.bindValue(":skill_prof", QJsonDocument(QJsonArray::fromStringList(background.skillProficiencies)).toJson(QJsonDocument::Compact));
    query.bindValue(":tool_prof", QJsonDocument(QJsonArray::fromStringList(background.toolProficiencies)).toJson(QJsonDocument::Compact));
    query.bindValue(":languages", QJsonDocument(QJsonArray::fromStringList(background.languages)).toJson(QJsonDocument::Compact));
    query.bindValue(":equipment", QJsonDocument(QJsonArray::fromStringList(background.equipment)).toJson(QJsonDocument::Compact));
    query.bindValue(":feature_name", background.featureName);
    query.bindValue(":feature_description", background.featureDescription);

    QJsonObject traitsObj;
    for (auto it = background.traits.begin(); it != background.traits.end(); ++it) {
        traitsObj.insert(it.key(), it.value());
    }
    query.bindValue(":traits", QJsonDocument(traitsObj).toJson(QJsonDocument::Compact));

    if (!query.exec()) {
        qDebug() << "Error adding background:" << query.lastError().text();
        return false;
    }

    return true;
}

QList<Background> DatabaseManager::getAllBackgrounds()
{
    QList<Background> list;
    QSqlQuery query("SELECT * FROM backgrounds ORDER BY name COLLATE NOCASE");
    while (query.next()) {
        Background background;
        background.slug = query.value("slug").toString();
        background.name = query.value("name").toString();
        background.source = query.value("source").toString();
        background.description = query.value("description").toString();
        background.featureName = query.value("feature_name").toString();
        background.featureDescription = query.value("feature_description").toString();

        const QJsonArray skillArray = QJsonDocument::fromJson(query.value("skill_prof").toString().toUtf8()).array();
        for (const QJsonValue &value : skillArray) {
            background.skillProficiencies << value.toString();
        }

        const QJsonArray toolArray = QJsonDocument::fromJson(query.value("tool_prof").toString().toUtf8()).array();
        for (const QJsonValue &value : toolArray) {
            background.toolProficiencies << value.toString();
        }

        const QJsonArray languageArray = QJsonDocument::fromJson(query.value("languages").toString().toUtf8()).array();
        for (const QJsonValue &value : languageArray) {
            background.languages << value.toString();
        }

        const QJsonArray equipmentArray = QJsonDocument::fromJson(query.value("equipment").toString().toUtf8()).array();
        for (const QJsonValue &value : equipmentArray) {
            background.equipment << value.toString();
        }

        const QJsonObject traitsObj = QJsonDocument::fromJson(query.value("traits").toString().toUtf8()).object();
        for (auto it = traitsObj.begin(); it != traitsObj.end(); ++it) {
            background.traits.insert(it.key(), it.value().toString());
        }

        list.append(background);
    }

    return list;
}

void DatabaseManager::importBackgroundsFromJson(const QString& filePath)
{
    QString resolvedPath = filePath;
    if (!QFile::exists(resolvedPath)) {
        QDir dir(QCoreApplication::applicationDirPath());
        if (dir.cdUp() && dir.cdUp() && dir.cdUp()) {
            const QString candidate = dir.filePath(filePath);
            if (QFile::exists(candidate)) {
                resolvedPath = candidate;
            }
        }
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open Backgrounds JSON:" << resolvedPath;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return;
    }

    QSqlDatabase::database().transaction();
    QSqlQuery("DELETE FROM backgrounds").exec();

    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        Background background;
        background.slug = obj.value("slug").toString();
        background.name = obj.value("name").toString();
        background.source = obj.value("source").toString();
        background.description = obj.value("description").toString();
        background.featureName = obj.value("featureName").toString();
        background.featureDescription = obj.value("featureDescription").toString();

        const QJsonArray skillArray = obj.value("skillProficiencies").toArray();
        for (const QJsonValue &entry : skillArray) {
            background.skillProficiencies << entry.toString();
        }

        const QJsonArray toolArray = obj.value("toolProficiencies").toArray();
        for (const QJsonValue &entry : toolArray) {
            background.toolProficiencies << entry.toString();
        }

        const QJsonArray languageArray = obj.value("languages").toArray();
        for (const QJsonValue &entry : languageArray) {
            background.languages << entry.toString();
        }

        const QJsonArray equipmentArray = obj.value("equipment").toArray();
        for (const QJsonValue &entry : equipmentArray) {
            background.equipment << entry.toString();
        }

        const QJsonObject traitsObj = obj.value("traits").toObject();
        for (auto it = traitsObj.begin(); it != traitsObj.end(); ++it) {
            background.traits.insert(it.key(), it.value().toString());
        }

        addBackground(background);
    }

    QSqlDatabase::database().commit();
    qDebug() << "Backgrounds imported from" << resolvedPath;
}

bool DatabaseManager::addFeat(const Feat& feat)
{
    QSqlQuery query;
    query.prepare(
        "INSERT INTO feats (slug, name, source, description, prerequisite, benefits) "
        "VALUES (:slug, :name, :source, :description, :prerequisite, :benefits)");

    query.bindValue(":slug", feat.slug);
    query.bindValue(":name", feat.name);
    query.bindValue(":source", feat.source);
    query.bindValue(":description", feat.description);
    query.bindValue(":prerequisite", feat.prerequisite);
    query.bindValue(":benefits", QJsonDocument(QJsonArray::fromStringList(feat.benefits)).toJson(QJsonDocument::Compact));

    if (!query.exec()) {
        qDebug() << "Error adding feat:" << query.lastError().text();
        return false;
    }

    return true;
}

QList<Feat> DatabaseManager::getAllFeats()
{
    QList<Feat> list;
    QSqlQuery query("SELECT * FROM feats ORDER BY name COLLATE NOCASE");
    while (query.next()) {
        Feat feat;
        feat.slug = query.value("slug").toString();
        feat.name = query.value("name").toString();
        feat.source = query.value("source").toString();
        feat.description = query.value("description").toString();
        feat.prerequisite = query.value("prerequisite").toString();

        const QJsonArray benefitArray = QJsonDocument::fromJson(query.value("benefits").toString().toUtf8()).array();
        for (const QJsonValue &entry : benefitArray) {
            feat.benefits << entry.toString();
        }

        list.append(feat);
    }

    return list;
}

void DatabaseManager::importFeatsFromJson(const QString& filePath)
{
    QString resolvedPath = filePath;
    if (!QFile::exists(resolvedPath)) {
        QDir dir(QCoreApplication::applicationDirPath());
        if (dir.cdUp() && dir.cdUp() && dir.cdUp()) {
            const QString candidate = dir.filePath(filePath);
            if (QFile::exists(candidate)) {
                resolvedPath = candidate;
            }
        }
    }

    QFile file(resolvedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Cannot open Feats JSON:" << resolvedPath;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return;
    }

    QSqlDatabase::database().transaction();
    QSqlQuery("DELETE FROM feats").exec();

    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        Feat feat;
        feat.slug = obj.value("slug").toString();
        feat.name = obj.value("name").toString();
        feat.source = obj.value("source").toString();
        feat.description = obj.value("description").toString();
        feat.prerequisite = obj.value("prerequisite").toString();

        const QJsonArray benefitArray = obj.value("benefits").toArray();
        for (const QJsonValue &entry : benefitArray) {
            feat.benefits << entry.toString();
        }

        addFeat(feat);
    }

    QSqlDatabase::database().commit();
    qDebug() << "Feats imported from" << resolvedPath;
}

bool DatabaseManager::saveCharacter(const QString& campaignName, const Character& character)
{
    const QString trimmedCampaign = campaignName.trimmed();
    if (trimmedCampaign.isEmpty()) {
        return false;
    }

    QSqlQuery query;
    query.prepare(
        "INSERT OR REPLACE INTO characters "
        "(campaign_name, name, level, race, class_summary, data, updated_at) "
        "VALUES (:campaign_name, :name, :level, :race, :class_summary, :data, CURRENT_TIMESTAMP)");
    query.bindValue(":campaign_name", trimmedCampaign);
    query.bindValue(":name", character.name());
    query.bindValue(":level", character.level);
    query.bindValue(":race", character.race());
    query.bindValue(":class_summary", character.characterClass());
    query.bindValue(":data", QJsonDocument(character.toJson()).toJson(QJsonDocument::Compact));

    if (!query.exec()) {
        qDebug() << "Error saving character:" << query.lastError().text();
        return false;
    }

    return true;
}

bool DatabaseManager::loadCharacter(const QString& campaignName, Character *character)
{
    if (!character) {
        return false;
    }

    const QString trimmedCampaign = campaignName.trimmed();
    if (trimmedCampaign.isEmpty()) {
        return false;
    }

    QSqlQuery query;
    query.prepare("SELECT data FROM characters WHERE campaign_name = :campaign_name LIMIT 1");
    query.bindValue(":campaign_name", trimmedCampaign);

    if (!query.exec()) {
        qDebug() << "Error loading character:" << query.lastError().text();
        return false;
    }

    if (!query.next()) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(query.value("data").toByteArray());
    if (!doc.isObject()) {
        qDebug() << "Stored character payload is invalid for campaign:" << trimmedCampaign;
        return false;
    }

    character->fromJson(doc.object());
    return true;
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

    auto hasColumn = [&](const QString &tableName, const QString &columnName) {
        QSqlQuery infoQuery;
        infoQuery.exec(QString("PRAGMA table_info(%1)").arg(tableName));
        while (infoQuery.next()) {
            if (infoQuery.value(1).toString() == columnName) {
                return true;
            }
        }
        return false;
    };
    
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

    if (hasColumn("races", "name") && !hasColumn("races", "slug")) {
        if (!query.exec("DROP TABLE IF EXISTS races")) {
            qDebug() << "Could not recreate races table:" << query.lastError();
            return false;
        }
    }

    // Таблица Рас
    QString createRacesTable = R"(
        CREATE TABLE IF NOT EXISTS races (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            slug TEXT UNIQUE,
            name TEXT,
            source TEXT,
            description TEXT,
            speed INTEGER,
            flying_speed INTEGER DEFAULT 0,
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

    if (hasColumn("classes", "name") && !hasColumn("classes", "slug")) {
        if (!query.exec("DROP TABLE IF EXISTS classes")) {
            qDebug() << "Could not recreate classes table:" << query.lastError();
            return false;
        }
    }

    // Таблица Классов
    QString createClassesTable = R"(
        CREATE TABLE IF NOT EXISTS classes (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            slug TEXT UNIQUE,
            name TEXT,
            source TEXT,
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

    QString createBackgroundsTable = R"(
        CREATE TABLE IF NOT EXISTS backgrounds (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            slug TEXT UNIQUE,
            name TEXT,
            source TEXT,
            description TEXT,
            skill_prof TEXT,
            tool_prof TEXT,
            languages TEXT,
            equipment TEXT,
            feature_name TEXT,
            feature_description TEXT,
            traits TEXT
        )
    )";

    if (!query.exec(createBackgroundsTable)) {
        qDebug() << "Could not create backgrounds table:" << query.lastError();
        return false;
    }

    QString createFeatsTable = R"(
        CREATE TABLE IF NOT EXISTS feats (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            slug TEXT UNIQUE,
            name TEXT,
            source TEXT,
            description TEXT,
            prerequisite TEXT,
            benefits TEXT
        )
    )";

    if (!query.exec(createFeatsTable)) {
        qDebug() << "Could not create feats table:" << query.lastError();
        return false;
    }

    QString createCharactersTable = R"(
        CREATE TABLE IF NOT EXISTS characters (
            campaign_name TEXT PRIMARY KEY,
            name TEXT,
            level INTEGER DEFAULT 1,
            race TEXT,
            class_summary TEXT,
            data TEXT NOT NULL,
            updated_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )";

    if (!query.exec(createCharactersTable)) {
        qDebug() << "Could not create characters table:" << query.lastError();
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
        "../../../scripts/final_item.json"
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
        "../../../scripts/final_bestiary.json"
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
