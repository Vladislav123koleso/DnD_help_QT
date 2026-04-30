#include "race_selection_page.h"
#include <QEvent>
#include <QEnterEvent>
#include <QPixmap>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QPushButton>
#include <algorithm>

#include "flowlayout.h"

// ---------------------------------------------------------

namespace {

QString selectionCardDescription(const Race &race)
{
    QString text = race.description.simplified();
    if (text.isEmpty() && !race.traits.isEmpty()) {
        text = race.traits.first().simplified();
    }
    if (text.length() > 90) {
        text = text.left(87) + "...";
    }
    if (text.isEmpty()) {
        text = QStringLiteral("Описание отсутствует");
    }
    return text;
}

class SubraceSelectionDialog : public QDialog
{
public:
    SubraceSelectionDialog(
        const QString &baseRaceName,
        const QMap<QString, Race> &subraceChoices,
        QWidget *parent = nullptr)
        : QDialog(parent),
          m_subraceChoices(subraceChoices)
    {
        setWindowTitle(QStringLiteral("Выбор подрасы"));
        resize(1100, 760);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        m_stackedWidget = new QStackedWidget(this);
        mainLayout->addWidget(m_stackedWidget);

        m_listPage = new QWidget(this);
        QVBoxLayout *listLayout = new QVBoxLayout(m_listPage);

        QHBoxLayout *headerLayout = new QHBoxLayout();
        QPushButton *cancelButton = new QPushButton(QStringLiteral("Отмена"), m_listPage);
        QLabel *titleLabel = new QLabel(QStringLiteral("Выбор подрасы для расы «%1»").arg(baseRaceName), m_listPage);
        titleLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold;"));
        headerLayout->addWidget(cancelButton);
        headerLayout->addStretch();
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch();
        listLayout->addLayout(headerLayout);

        QScrollArea *scrollArea = new QScrollArea(m_listPage);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);

        QWidget *scrollContent = new QWidget(scrollArea);
        FlowLayout *contentLayout = new FlowLayout(scrollContent, 20, 20, 20);

        QStringList subraceNames = m_subraceChoices.keys();
        std::sort(subraceNames.begin(), subraceNames.end(), [](const QString &left, const QString &right) {
            return left.localeAwareCompare(right) < 0;
        });

        for (const QString &subraceName : subraceNames) {
            const Race race = m_subraceChoices.value(subraceName);
            RaceCard *card = new RaceCard(
                race.name,
                race.imagePath,
                race.imagePath,
                selectionCardDescription(race),
                scrollContent);
            connect(card, &RaceCard::raceSelected, this, [this, subraceName](const QString &) {
                showDetails(subraceName);
            });
            contentLayout->addWidget(card);
        }

        scrollArea->setWidget(scrollContent);
        listLayout->addWidget(scrollArea);

        m_detailsPage = new RaceDetailsPage(this);
        connect(m_detailsPage, &RaceDetailsPage::backClicked, this, [this]() {
            m_stackedWidget->setCurrentWidget(m_listPage);
        });
        connect(m_detailsPage, &RaceDetailsPage::continueClicked, this, [this]() {
            if (m_selectedSubraceName.trimmed().isEmpty()) {
                return;
            }
            m_selectedRace = m_subraceChoices.value(m_selectedSubraceName);
            accept();
        });

        m_stackedWidget->addWidget(m_listPage);
        m_stackedWidget->addWidget(m_detailsPage);
        m_stackedWidget->setCurrentWidget(m_listPage);

        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    }

    Race selectedRace() const
    {
        return m_selectedRace;
    }

private:
    void showDetails(const QString &subraceName)
    {
        m_selectedSubraceName = subraceName;
        m_detailsPage->setRace(m_subraceChoices.value(subraceName));
        m_stackedWidget->setCurrentWidget(m_detailsPage);
    }

    QMap<QString, Race> m_subraceChoices;
    QString m_selectedSubraceName;
    Race m_selectedRace;
    QStackedWidget *m_stackedWidget = nullptr;
    QWidget *m_listPage = nullptr;
    RaceDetailsPage *m_detailsPage = nullptr;
};

}

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
        race.hasType = obj.contains("type");
        race.type = obj.value("type").toString(QStringLiteral("race")).trimmed().toLower();
        if (race.type.isEmpty()) {
            race.type = QStringLiteral("race");
        }
        race.hasSlug = obj.contains("slug");
        race.slug = obj.value("slug").toString().trimmed();
        race.name = obj.value("name").toString().trimmed();
        race.hasSource = obj.contains("source");
        race.source = obj.value("source").toString().trimmed();
        race.hasDescription = obj.contains("description");
        race.description = obj.value("description").toString().trimmed();
        race.hasSpeed = obj.contains("speed");
        race.speed = obj.value("speed").toInt(30);
        race.hasFlyingSpeed = obj.contains("flyingSpeed");
        race.flyingSpeed = obj.value("flyingSpeed").toInt(0);
        race.hasSize = obj.contains("size");
        race.size = obj.value("size").toString("Средний");
        race.imagePath = detectImagePath(race.name);

        const QJsonObject asi = obj.value("asi").toObject();
        race.hasAbilityScoreIncrease = obj.contains("asi") && !asi.isEmpty();
        race.abilityScoreIncrease["Strength"] = asi.value("str").toInt(0);
        race.abilityScoreIncrease["Dexterity"] = asi.value("dex").toInt(0);
        race.abilityScoreIncrease["Constitution"] = asi.value("con").toInt(0);
        race.abilityScoreIncrease["Intelligence"] = asi.value("int").toInt(0);
        race.abilityScoreIncrease["Wisdom"] = asi.value("wis").toInt(0);
        race.abilityScoreIncrease["Charisma"] = asi.value("cha").toInt(0);

        const QJsonObject traitsObj = obj.value("traits").toObject();
        race.hasTraits = obj.contains("traits") && !traitsObj.isEmpty();
        for (auto it = traitsObj.begin(); it != traitsObj.end(); ++it) {
            race.traits.insert(it.key(), it.value().toString());
        }

        const QJsonArray langs = obj.value("languages").toArray();
        race.hasLanguages = obj.contains("languages") && !langs.isEmpty();
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
    QString basePath;
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (dir.exists("DndHelperDesign/DndHelperDesignContent/images/race")) {
            basePath = dir.filePath("DndHelperDesign/DndHelperDesignContent/images/race/");
            break;
        }
        if (!dir.cdUp()) break;
    }
    
    if (basePath.isEmpty()) {
        basePath = "DndHelperDesign/DndHelperDesignContent/images/race/";
    }

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
    return selectionCardDescription(race);
}

QMap<QString, QString> RaceSelectionPage::displayTraitsForParentRace(const Race &race) const
{
    if (race.name != QStringLiteral("Эльф")) {
        return race.traits;
    }

    static const QStringList allowedTitles = {
        QStringLiteral("Увеличение характеристик"),
        QStringLiteral("Тёмное зрение"),
        QStringLiteral("Обострённые чувства"),
        QStringLiteral("Наследие фей"),
        QStringLiteral("Транс")
    };

    QMap<QString, QString> filtered;
    for (auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        if (allowedTitles.contains(it.key())) {
            filtered.insert(it.key(), it.value());
        }
    }
    return filtered;
}

Race RaceSelectionPage::displayRaceForSelection(const QString &raceName) const
{
    Race race = raceData.value(raceName);
    if (!subraceGroups.contains(raceName)) {
        const QString parentRaceName = parentRaceNameForSubrace(raceName);
        if (!parentRaceName.isEmpty()) {
            return mergeRaceWithSubrace(raceData.value(parentRaceName), race);
        }
        return race;
    }

    race.traits = displayTraitsForParentRace(race);
    if (raceName == QStringLiteral("Эльф")) {
        const QString note = QStringLiteral("\n\nПри продолжении выберите конкретную подрасу эльфа.");
        if (!race.description.contains(note)) {
            race.description += note;
        }
    }
    return race;
}

QString RaceSelectionPage::parentRaceNameForSubrace(const QString &raceName) const
{
    const QString trimmedRaceName = raceName.trimmed();
    for (auto it = subraceGroups.begin(); it != subraceGroups.end(); ++it) {
        if (it.key().compare(trimmedRaceName, Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (it.value().contains(trimmedRaceName, Qt::CaseInsensitive)) {
            return it.key();
        }
    }
    return QString();
}

Race RaceSelectionPage::mergeRaceWithSubrace(const Race &baseRace, const Race &subrace) const
{
    Race merged = baseRace;

    if (subrace.hasSlug && !subrace.slug.trimmed().isEmpty()) {
        merged.slug = subrace.slug;
        merged.hasSlug = true;
    }

    if (!subrace.name.trimmed().isEmpty()) {
        merged.name = subrace.name.trimmed();
    }

    if (subrace.hasType && !subrace.type.trimmed().isEmpty()) {
        merged.type = subrace.type.trimmed().toLower();
        merged.hasType = true;
    }

    if (subrace.hasSource && !subrace.source.trimmed().isEmpty()) {
        merged.source = subrace.source.trimmed();
        merged.hasSource = true;
    }

    if (subrace.hasDescription && !subrace.description.trimmed().isEmpty()) {
        const QString subraceDescription = subrace.description.trimmed();
        if (merged.description.trimmed().isEmpty()) {
            merged.description = subraceDescription;
        } else if (merged.description.trimmed() != subraceDescription) {
            merged.description += QStringLiteral("\n\n") + subraceDescription;
        }
        merged.hasDescription = true;
    }

    if (subrace.hasSize && !subrace.size.trimmed().isEmpty()) {
        merged.size = subrace.size.trimmed();
        merged.hasSize = true;
    }

    if (subrace.hasSpeed) {
        merged.speed = subrace.speed;
        merged.hasSpeed = true;
    }

    if (subrace.hasFlyingSpeed) {
        merged.flyingSpeed = subrace.flyingSpeed;
        merged.hasFlyingSpeed = true;
    }

    if (subrace.hasAbilityScoreIncrease) {
        for (auto it = subrace.abilityScoreIncrease.begin(); it != subrace.abilityScoreIncrease.end(); ++it) {
            merged.abilityScoreIncrease[it.key()] = merged.abilityScoreIncrease.value(it.key(), 0) + it.value();
        }
        merged.hasAbilityScoreIncrease = true;
    }

    if (subrace.hasTraits) {
        for (auto it = subrace.traits.begin(); it != subrace.traits.end(); ++it) {
            merged.traits.insert(it.key(), it.value());
        }
        merged.hasTraits = true;
    }

    if (subrace.hasLanguages) {
        for (const QString &language : subrace.languages) {
            if (!language.trimmed().isEmpty() && !merged.languages.contains(language)) {
                merged.languages << language;
            }
        }
        merged.hasLanguages = true;
    }

    if (!subrace.imagePath.trimmed().isEmpty()) {
        merged.imagePath = subrace.imagePath;
    }

    if (!baseRace.subraces.isEmpty()) {
        merged.subraces = baseRace.subraces;
    } else {
        merged.subraces = subrace.subraces;
    }

    if (merged.description.trimmed().isEmpty()) {
        if (!merged.traits.isEmpty()) {
            merged.description = merged.traits.first();
        } else {
            merged.description = QStringLiteral("Описание отсутствует.");
        }
    }

    return merged;
}

QStringList RaceSelectionPage::subraceOptionsForRace(const QString &raceName) const
{
    QStringList options = subraceGroups.value(raceName);
    if (raceName == QStringLiteral("Эльф")) {
        options.removeAll(raceName);
    }
    return options;
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
    raceNames.erase(std::remove_if(raceNames.begin(), raceNames.end(), [this](const QString &raceName) {
        const QString raceType = raceData.value(raceName).type.trimmed().toLower();
        return raceType == QStringLiteral("subrace");
    }), raceNames.end());
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
        detailsPage->setRace(displayRaceForSelection(raceName));
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
    Race selectedRace = detailsPage->currentRace();
    const QString selectedName = selectedRace.name.trimmed();
    const QStringList options = subraceOptionsForRace(selectedName);

    if (options.size() > 1) {
        const Race baseRace = raceData.value(selectedName, selectedRace);
        QMap<QString, Race> subraceChoices;
        for (const QString &subraceName : options) {
            const Race subrace = raceData.value(subraceName);
            if (subrace.name.trimmed().isEmpty()) {
                continue;
            }
            subraceChoices.insert(subraceName, mergeRaceWithSubrace(baseRace, subrace));
        }

        SubraceSelectionDialog dialog(selectedName, subraceChoices, this);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        selectedRace = dialog.selectedRace();
    }

    emit raceChosen(selectedRace);
}

Race RaceSelectionPage::getRaceData(const QString &name)
{
    if (raceData.contains(name)) {
        return raceData[name];
    }
    return Race();
}


