#include "race_selection_page.h"
#include <QEvent>
#include <QEnterEvent>
#include <QPixmap>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

#include "flowlayout.h"

// ---------------------------------------------------------

RaceSelectionPage::RaceSelectionPage(QWidget *parent) : QWidget(parent) {
    loadRaceData();
    setupUi();
}

void RaceSelectionPage::loadRaceData()
{
    raceData.clear();

    const QString jsonPath = resolveRacesJsonPath();
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Не удалось открыть races_dndsu.json:" << jsonPath;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        qDebug() << "Некорректный формат races_dndsu.json";
        return;
    }

    const QJsonArray racesArray = doc.array();
    for (const QJsonValue &val : racesArray) {
        const QJsonObject obj = val.toObject();

        Race race;
        race.name = obj.value("name").toString().trimmed();
        race.description = obj.value("description").toString().trimmed();
        race.speed = obj.value("speed").toInt(30);
        race.flyingSpeed = obj.value("flyingSpeed").toInt(0);
        race.size = obj.value("size").toString("Средний");
        race.imagePath = detectImagePath(race.name);

        const QJsonObject asi = obj.value("asi").toObject();
        race.abilityScoreIncrease["Strength"] = asi.value("str").toInt(0);
        race.abilityScoreIncrease["Dexterity"] = asi.value("dex").toInt(0);
        race.abilityScoreIncrease["Constitution"] = asi.value("con").toInt(0);
        race.abilityScoreIncrease["Intelligence"] = asi.value("int").toInt(0);
        race.abilityScoreIncrease["Wisdom"] = asi.value("wis").toInt(0);
        race.abilityScoreIncrease["Charisma"] = asi.value("cha").toInt(0);

        const QJsonObject traitsObj = obj.value("traits").toObject();
        for (auto it = traitsObj.begin(); it != traitsObj.end(); ++it) {
            race.traits.insert(it.key(), it.value().toString());
        }

        const QJsonArray langs = obj.value("languages").toArray();
        for (const QJsonValue &lang : langs) {
            const QString s = lang.toString().trimmed();
            if (!s.isEmpty()) {
                race.languages << s;
            }
        }

        if (race.description.isEmpty()) {
            if (!race.traits.isEmpty()) {
                race.description = race.traits.first();
            } else {
                race.description = "Описание отсутствует.";
            }
        }

        if (!race.name.isEmpty()) {
            raceData[race.name] = race;
        }
    }

    buildSubraceGroups();
}

QString RaceSelectionPage::resolveRacesJsonPath() const
{
    QStringList candidates;
    candidates << "races_dndsu.json";
    candidates << QDir::current().filePath("races_dndsu.json");

    QDir appDir(QCoreApplication::applicationDirPath());
    candidates << appDir.filePath("races_dndsu.json");

    QDir dir = appDir;
    for (int i = 0; i < 6; ++i) {
        candidates << dir.filePath("races_dndsu.json");
        if (!dir.cdUp()) {
            break;
        }
    }

    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return "races_dndsu.json";
}

QString RaceSelectionPage::resolveSubracesMapPath() const
{
    QStringList candidates;
    candidates << "data/subraces_map.json";
    candidates << QDir::current().filePath("data/subraces_map.json");

    QDir appDir(QCoreApplication::applicationDirPath());
    candidates << appDir.filePath("data/subraces_map.json");

    QDir dir = appDir;
    for (int i = 0; i < 6; ++i) {
        candidates << dir.filePath("data/subraces_map.json");
        if (!dir.cdUp()) {
            break;
        }
    }

    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return "data/subraces_map.json";
}

QString RaceSelectionPage::detectImagePath(const QString &raceName) const
{
    const QString basePath = "D:/repos/qt/DnD_help/DndHelperDesign/DndHelperDesignContent/images/race/";

    const QMap<QString, QString> explicitMap = {
        {"Эльф", "elf.jpg"},
        {"Человек", "human.jpg"},
        {"Гном", "gnome.jpg"},
        {"Ааракокра", "aarakocra.jpg"}
    };

    if (explicitMap.contains(raceName)) {
        const QString path = basePath + explicitMap.value(raceName);
        if (QFile::exists(path)) {
            return path;
        }
    }

    const QString fallback = basePath + "human.jpg";
    if (QFile::exists(fallback)) {
        return fallback;
    }

    return QString();
}

QString RaceSelectionPage::shortDescription(const Race &race) const
{
    QString text = race.description.simplified();
    if (text.isEmpty() && !race.traits.isEmpty()) {
        text = race.traits.first().simplified();
    }
    if (text.length() > 90) {
        text = text.left(87) + "...";
    }
    if (text.isEmpty()) {
        text = "Описание отсутствует";
    }
    return text;
}

void RaceSelectionPage::buildSubraceGroups()
{
    loadSubracesMapFromJson();

    for (auto it = raceData.begin(); it != raceData.end(); ++it) {
        QStringList list;
        for (auto g = subraceGroups.begin(); g != subraceGroups.end(); ++g) {
            if (g.value().contains(it.key(), Qt::CaseInsensitive)) {
                list = g.value();
                break;
            }
        }
        it->subraces = list;
    }
}

void RaceSelectionPage::loadSubracesMapFromJson()
{
    subraceGroups.clear();

    const QString mapPath = resolveSubracesMapPath();
    QFile file(mapPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Не удалось открыть subraces_map.json:" << mapPath;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qDebug() << "Некорректный формат subraces_map.json";
        return;
    }

    const QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QString parent = it.key().trimmed();
        const QJsonArray arr = it.value().toArray();

        QStringList values;
        for (const QJsonValue &v : arr) {
            const QString subraceName = v.toString().trimmed();
            if (!subraceName.isEmpty() && raceData.contains(subraceName)) {
                values << subraceName;
            }
        }

        values.removeDuplicates();
        if (values.size() > 1 && !parent.isEmpty()) {
            subraceGroups[parent] = values;
        }
    }
}

void RaceSelectionPage::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    stackedWidget = new QStackedWidget(this);
    
    // Page 1: List
    listPage = new QWidget();
    QVBoxLayout *listLayout = new QVBoxLayout(listPage);
    
    QScrollArea *scrollArea = new QScrollArea(listPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    scrollContent = new QWidget();
    // Use FlowLayout for cards
    FlowLayout *contentLayout = new FlowLayout(scrollContent, 20, 20, 20);

    QStringList raceNames = raceData.keys();
    std::sort(raceNames.begin(), raceNames.end(), [](const QString &a, const QString &b) {
        return a.localeAwareCompare(b) < 0;
    });

    for (const QString &raceName : raceNames) {
        const Race race = raceData.value(raceName);
        const QString normalImage = race.imagePath;
        const QString hoverImage = race.imagePath;
        RaceCard *card = new RaceCard(raceName, normalImage, hoverImage, shortDescription(race), scrollContent);
        connect(card, &RaceCard::raceSelected, this, [this](const QString &name){ onRaceSelected(name); });
        contentLayout->addWidget(card);
    }

    scrollArea->setWidget(scrollContent);
    listLayout->addWidget(scrollArea);
    
    // Page 2: Details
    detailsPage = new RaceDetailsPage();
    connect(detailsPage, &RaceDetailsPage::backClicked, this, &RaceSelectionPage::showList);
    connect(detailsPage, &RaceDetailsPage::continueClicked, this, &RaceSelectionPage::confirmSelection);

    stackedWidget->addWidget(listPage);
    stackedWidget->addWidget(detailsPage);
    
    mainLayout->addWidget(stackedWidget);
}

void RaceSelectionPage::onRaceSelected(const QString &raceName)
{
    if (raceData.contains(raceName)) {
        detailsPage->setRace(raceData[raceName]);
        stackedWidget->setCurrentWidget(detailsPage);
    } else {
        qDebug() << "Race data not found for:" << raceName;
    }
}

void RaceSelectionPage::showList()
{
    stackedWidget->setCurrentWidget(listPage);
}

void RaceSelectionPage::confirmSelection()
{
    emit raceChosen(detailsPage->currentRace());
}

Race RaceSelectionPage::getRaceData(const QString &name)
{
    if (raceData.contains(name)) {
        return raceData[name];
    }
    return Race();
}


