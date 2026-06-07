#include "namegeneratorwidget.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QSplitter>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QComboBox>
#include <QtGlobal>
#include <algorithm>

namespace {

QStringList jsonStringList(const QJsonValue &value)
{
    QStringList values;
    const QJsonArray array = value.toArray();
    for (const QJsonValue &entry : array) {
        const QString text = entry.toString().trimmed();
        if (!text.isEmpty()) {
            values << text;
        }
    }
    values.removeDuplicates();
    return values;
}

double jsonChance(const QJsonValue &value, double fallback)
{
    if (!value.isDouble()) {
        return fallback;
    }
    return qBound(0.0, value.toDouble(), 1.0);
}

} // namespace

NameGeneratorWidget::NameGeneratorWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("NameGeneratorWidget"));
    setupUi();

    if (!loadData()) {
        statusLabel->setText(QStringLiteral("Не удалось загрузить базы генератора. Проверьте JSON в папке data/."));
        statusLabel->setStyleSheet(QStringLiteral("color: #9d3a34;"));
    } else {
        statusLabel->setText(QStringLiteral("Базы генератора загружены."));
        statusLabel->setStyleSheet(QStringLiteral("color: #3f6f4f;"));
    }

    fillRaceCombo();
    fillTerrainCombo();
    onPlaceTypeChanged();

    connect(generateNamesBtn, &QPushButton::clicked, this, &NameGeneratorWidget::generateCharacterNames);
    connect(generatePlacesBtn, &QPushButton::clicked, this, &NameGeneratorWidget::generateLocationNames);
    connect(placeTypeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int) { onPlaceTypeChanged(); });
}

void NameGeneratorWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(12);

    QSplitter *contentSplitter = new QSplitter(Qt::Vertical, this);
    contentSplitter->setChildrenCollapsible(false);

    QGroupBox *nameGroup = new QGroupBox(QStringLiteral("Генерация имён персонажей"), this);
    QVBoxLayout *nameGroupLayout = new QVBoxLayout(nameGroup);

    QFormLayout *nameForm = new QFormLayout();
    genderCombo = new QComboBox(nameGroup);
    genderCombo->addItem(QStringLiteral("Любой"), QStringLiteral("any"));
    genderCombo->addItem(QStringLiteral("Мужской"), QStringLiteral("male"));
    genderCombo->addItem(QStringLiteral("Женский"), QStringLiteral("female"));

    raceCombo = new QComboBox(nameGroup);
    nameCountSpin = new QSpinBox(nameGroup);
    nameCountSpin->setRange(1, 100);
    nameCountSpin->setValue(10);

    nameForm->addRow(QStringLiteral("Пол:"), genderCombo);
    nameForm->addRow(QStringLiteral("Раса:"), raceCombo);
    nameForm->addRow(QStringLiteral("Количество имён:"), nameCountSpin);
    nameGroupLayout->addLayout(nameForm);

    generateNamesBtn = new QPushButton(QStringLiteral("Сгенерировать имена"), nameGroup);
    generateNamesBtn->setProperty("variant", QStringLiteral("accent"));
    nameGroupLayout->addWidget(generateNamesBtn);

    namesOutput = new QPlainTextEdit(nameGroup);
    namesOutput->setReadOnly(true);
    namesOutput->setPlaceholderText(QStringLiteral("Здесь появится список сгенерированных имён."));
    namesOutput->setMinimumHeight(130);
    nameGroupLayout->addWidget(namesOutput, 1);

    QGroupBox *placeGroup = new QGroupBox(QStringLiteral("Генерация названий локаций"), this);
    QVBoxLayout *placeGroupLayout = new QVBoxLayout(placeGroup);

    QFormLayout *placeForm = new QFormLayout();
    placeTypeCombo = new QComboBox(placeGroup);
    placeTypeCombo->addItem(QStringLiteral("Город"), QStringLiteral("city"));
    placeTypeCombo->addItem(QStringLiteral("Деревня"), QStringLiteral("village"));
    placeTypeCombo->addItem(QStringLiteral("Порт"), QStringLiteral("port"));
    placeTypeCombo->addItem(QStringLiteral("Местность"), QStringLiteral("terrain"));

    terrainTypeLabel = new QLabel(QStringLiteral("Тип местности:"), placeGroup);
    terrainTypeCombo = new QComboBox(placeGroup);

    placeCountSpin = new QSpinBox(placeGroup);
    placeCountSpin->setRange(1, 100);
    placeCountSpin->setValue(10);

    placeForm->addRow(QStringLiteral("Тип:"), placeTypeCombo);
    placeForm->addRow(terrainTypeLabel, terrainTypeCombo);
    placeForm->addRow(QStringLiteral("Количество названий:"), placeCountSpin);
    placeGroupLayout->addLayout(placeForm);

    generatePlacesBtn = new QPushButton(QStringLiteral("Сгенерировать названия"), placeGroup);
    generatePlacesBtn->setProperty("variant", QStringLiteral("accent"));
    placeGroupLayout->addWidget(generatePlacesBtn);

    placesOutput = new QPlainTextEdit(placeGroup);
    placesOutput->setReadOnly(true);
    placesOutput->setPlaceholderText(QStringLiteral("Здесь появится список названий городов, деревень и местностей."));
    placesOutput->setMinimumHeight(130);
    placeGroupLayout->addWidget(placesOutput, 1);

    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);

    contentSplitter->addWidget(nameGroup);
    contentSplitter->addWidget(placeGroup);
    contentSplitter->setStretchFactor(0, 1);
    contentSplitter->setStretchFactor(1, 1);
    contentSplitter->setSizes({430, 430});

    mainLayout->addWidget(contentSplitter, 1);
    mainLayout->addWidget(statusLabel);
}
QString NameGeneratorWidget::resolveDataPath(const QString &relativePath) const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::current().filePath(relativePath),
        QDir(appDir).filePath(relativePath),
        QDir(appDir).filePath(QStringLiteral("../") + relativePath),
        QDir(appDir).filePath(QStringLiteral("../../") + relativePath),
        QDir(appDir).filePath(QStringLiteral("../../../") + relativePath)
    };

    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }
    return QString();
}

bool NameGeneratorWidget::loadData()
{
    m_raceData.clear();
    m_settlementData.clear();
    m_terrainData.clear();
    m_fallbackRaceKey.clear();

    const QString namePath = resolveDataPath(QStringLiteral("data/fantasy_names.json"));
    const QString placePath = resolveDataPath(QStringLiteral("data/fantasy_places.json"));
    if (namePath.isEmpty() || placePath.isEmpty()) {
        return false;
    }

    const bool namesLoaded = loadNameData(namePath);
    const bool placesLoaded = loadPlaceData(placePath);
    return namesLoaded && placesLoaded;
}

bool NameGeneratorWidget::loadNameData(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();
    m_fallbackRaceKey = root.value(QStringLiteral("fallbackRace")).toString();

    const QJsonObject racesObj = root.value(QStringLiteral("races")).toObject();
    for (auto it = racesObj.begin(); it != racesObj.end(); ++it) {
        const QString raceKey = it.key().trimmed();
        const QJsonObject raceObj = it.value().toObject();
        if (raceKey.isEmpty() || raceObj.isEmpty()) {
            continue;
        }

        RaceNameData raceData;
        raceData.displayName = raceObj.value(QStringLiteral("displayName")).toString(raceKey);
        raceData.surnames = jsonStringList(raceObj.value(QStringLiteral("surnames")));
        raceData.nicknames = jsonStringList(raceObj.value(QStringLiteral("nicknames")));
        raceData.honorifics = jsonStringList(raceObj.value(QStringLiteral("honorifics")));
        raceData.origins = jsonStringList(raceObj.value(QStringLiteral("origins")));
        raceData.formats = jsonStringList(raceObj.value(QStringLiteral("formats")));
        raceData.surnameChance = jsonChance(raceObj.value(QStringLiteral("surnameChance")), raceData.surnameChance);
        raceData.nicknameChance = jsonChance(raceObj.value(QStringLiteral("nicknameChance")), raceData.nicknameChance);
        raceData.honorificChance = jsonChance(raceObj.value(QStringLiteral("honorificChance")), raceData.honorificChance);

        const QStringList genderKeys = {
            QStringLiteral("male"),
            QStringLiteral("female"),
            QStringLiteral("unisex")
        };

        for (const QString &genderKey : genderKeys) {
            const QJsonObject poolObj = raceObj.value(genderKey).toObject();
            if (poolObj.isEmpty()) {
                continue;
            }

            NamePool pool;
            pool.full = jsonStringList(poolObj.value(QStringLiteral("full")));
            pool.prefixes = jsonStringList(poolObj.value(QStringLiteral("prefixes")));
            pool.middles = jsonStringList(poolObj.value(QStringLiteral("middles")));
            pool.suffixes = jsonStringList(poolObj.value(QStringLiteral("suffixes")));
            raceData.poolsByGender.insert(genderKey, pool);
        }

        if (!raceData.poolsByGender.isEmpty()) {
            m_raceData.insert(raceKey, raceData);
        }
    }

    if (m_fallbackRaceKey.isEmpty() || !m_raceData.contains(m_fallbackRaceKey)) {
        m_fallbackRaceKey = m_raceData.isEmpty() ? QString() : m_raceData.firstKey();
    }
    return !m_raceData.isEmpty();
}

bool NameGeneratorWidget::loadPlaceData(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();

    const auto parseContainer = [](const QJsonObject &container, QMap<QString, PlaceNameData> &target) {
        for (auto it = container.begin(); it != container.end(); ++it) {
            const QString key = it.key().trimmed();
            const QJsonObject obj = it.value().toObject();
            if (key.isEmpty() || obj.isEmpty()) {
                continue;
            }

            PlaceNameData data;
            data.displayName = obj.value(QStringLiteral("displayName")).toString(key);
            data.prefixes = jsonStringList(obj.value(QStringLiteral("prefixes")));
            data.cores = jsonStringList(obj.value(QStringLiteral("cores")));
            data.suffixes = jsonStringList(obj.value(QStringLiteral("suffixes")));
            data.adjectives = jsonStringList(obj.value(QStringLiteral("adjectives")));
            data.nouns = jsonStringList(obj.value(QStringLiteral("nouns")));
            data.genitives = jsonStringList(obj.value(QStringLiteral("genitives")));
            data.formats = jsonStringList(obj.value(QStringLiteral("formats")));
            target.insert(key, data);
        }
    };

    parseContainer(root.value(QStringLiteral("settlements")).toObject(), m_settlementData);
    parseContainer(root.value(QStringLiteral("terrain")).toObject(), m_terrainData);

    return !m_settlementData.isEmpty() && !m_terrainData.isEmpty();
}

void NameGeneratorWidget::fillRaceCombo()
{
    raceCombo->clear();
    struct RaceEntry {
        QString key;
        QString displayName;
    };

    QList<RaceEntry> entries;
    for (auto it = m_raceData.constBegin(); it != m_raceData.constEnd(); ++it) {
        entries.append({it.key(), it.value().displayName});
    }
    std::sort(entries.begin(), entries.end(), [](const RaceEntry &left, const RaceEntry &right) {
        return left.displayName.localeAwareCompare(right.displayName) < 0;
    });

    for (const RaceEntry &entry : entries) {
        raceCombo->addItem(entry.displayName, entry.key);
    }

    const int fallbackIndex = raceCombo->findData(m_fallbackRaceKey);
    if (fallbackIndex >= 0) {
        raceCombo->setCurrentIndex(fallbackIndex);
    }
}

void NameGeneratorWidget::fillTerrainCombo()
{
    terrainTypeCombo->clear();

    struct TerrainEntry {
        QString key;
        QString displayName;
    };

    QList<TerrainEntry> entries;
    for (auto it = m_terrainData.constBegin(); it != m_terrainData.constEnd(); ++it) {
        entries.append({it.key(), it.value().displayName});
    }
    std::sort(entries.begin(), entries.end(), [](const TerrainEntry &left, const TerrainEntry &right) {
        return left.displayName.localeAwareCompare(right.displayName) < 0;
    });

    for (const TerrainEntry &entry : entries) {
        terrainTypeCombo->addItem(entry.displayName, entry.key);
    }
}

QString NameGeneratorWidget::randomFrom(const QStringList &values) const
{
    if (values.isEmpty()) {
        return QString();
    }
    const int index = QRandomGenerator::global()->bounded(values.size());
    return values.at(index);
}

QString NameGeneratorWidget::composeFromPool(const NamePool &pool) const
{
    const bool useFullName = !pool.full.isEmpty() && QRandomGenerator::global()->bounded(100) < 45;
    if (useFullName) {
        return randomFrom(pool.full);
    }

    QString prefix = randomFrom(pool.prefixes);
    QString middle;
    if (!pool.middles.isEmpty() && QRandomGenerator::global()->bounded(100) < 35) {
        middle = randomFrom(pool.middles);
    }
    QString suffix = randomFrom(pool.suffixes);

    QString result = prefix + middle + suffix;
    if (result.isEmpty()) {
        result = randomFrom(pool.full);
    }
    return result;
}

QString NameGeneratorWidget::chooseFirstName(const RaceNameData &raceData, const QString &genderKey) const
{
    const auto findPool = [&](const QString &key) -> const NamePool * {
        auto it = raceData.poolsByGender.constFind(key);
        if (it == raceData.poolsByGender.constEnd()) {
            return nullptr;
        }
        return &it.value();
    };

    const auto generateFromPool = [&](const NamePool *pool, int fullNameChancePercent) -> QString {
        if (!pool) {
            return QString();
        }

        const bool shouldUseFullName =
            !pool->full.isEmpty() &&
            (pool->prefixes.isEmpty() || pool->suffixes.isEmpty() ||
             QRandomGenerator::global()->bounded(100) < fullNameChancePercent);
        if (shouldUseFullName) {
            return randomFrom(pool->full);
        }

        QString composed = composeFromPool(*pool);
        if (!composed.isEmpty()) {
            return composed;
        }
        return randomFrom(pool->full);
    };

    const NamePool *malePool = findPool(QStringLiteral("male"));
    const NamePool *femalePool = findPool(QStringLiteral("female"));
    const NamePool *unisexPool = findPool(QStringLiteral("unisex"));

    if (genderKey == QStringLiteral("female")) {
        if (femalePool) {
            return generateFromPool(femalePool, 100);
        }
        if (unisexPool) {
            return generateFromPool(unisexPool, 100);
        }
        return generateFromPool(malePool, 100);
    }

    if (genderKey == QStringLiteral("male")) {
        if (malePool) {
            return generateFromPool(malePool, 88);
        }
        if (unisexPool) {
            return generateFromPool(unisexPool, 95);
        }
        return generateFromPool(femalePool, 95);
    }

    struct WeightedPool {
        const NamePool *pool = nullptr;
        int weight = 0;
        int fullNameChancePercent = 90;
    };

    QVector<WeightedPool> weightedPools;
    if (malePool) {
        weightedPools.append({malePool, 45, 88});
    }
    if (femalePool) {
        weightedPools.append({femalePool, 45, 92});
    }
    if (unisexPool) {
        weightedPools.append({unisexPool, 10, 95});
    }

    if (weightedPools.isEmpty()) {
        return QString();
    }

    int totalWeight = 0;
    for (const WeightedPool &entry : weightedPools) {
        totalWeight += entry.weight;
    }

    int roll = QRandomGenerator::global()->bounded(totalWeight);
    for (const WeightedPool &entry : weightedPools) {
        if (roll < entry.weight) {
            return generateFromPool(entry.pool, entry.fullNameChancePercent);
        }
        roll -= entry.weight;
    }

    return generateFromPool(weightedPools.last().pool, weightedPools.last().fullNameChancePercent);
}

QString NameGeneratorWidget::cleanupGeneratedText(QString text) const
{
    text.replace(QRegularExpression(QStringLiteral("\\{[^}]+\\}")), QString());
    text.replace(QRegularExpression(QStringLiteral("\\\"\\s*\\\"")), QString());
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral("\\s+,\\s*")), QStringLiteral(", "));
    text.replace(QRegularExpression(QStringLiteral("\\s+-\\s+")), QStringLiteral("-"));
    text.replace(QRegularExpression(QStringLiteral("\\s+\\\"")), QStringLiteral(" \""));
    text.replace(QRegularExpression(QStringLiteral("\\\"\\s+")), QStringLiteral("\" "));
    text = text.trimmed();

    if (text.startsWith(QLatin1Char(','))) {
        text.remove(0, 1);
    }
    if (text.endsWith(QLatin1Char(','))) {
        text.chop(1);
    }
    return text.trimmed();
}

QString NameGeneratorWidget::applyTemplate(QString templ, const QMap<QString, QString> &tokens) const
{
    if (templ.trimmed().isEmpty()) {
        templ = QStringLiteral("{first} {surname}");
    }
    for (auto it = tokens.begin(); it != tokens.end(); ++it) {
        templ.replace(QStringLiteral("{%1}").arg(it.key()), it.value());
    }
    return cleanupGeneratedText(templ);
}

QString NameGeneratorWidget::generateCharacterName(const RaceNameData &raceData, const QString &genderKey) const
{
    QString first = chooseFirstName(raceData, genderKey);
    if (first.isEmpty()) {
        first = QStringLiteral("Nameless");
    }

    QString surname;
    if (!raceData.surnames.isEmpty() && QRandomGenerator::global()->generateDouble() <= raceData.surnameChance) {
        surname = randomFrom(raceData.surnames);
    }

    // In female mode we avoid masculine epithets unless dedicated female forms are provided.
    const bool allowDecorativeTitles = genderKey != QStringLiteral("female");

    QString nickname;
    if (allowDecorativeTitles &&
        !raceData.nicknames.isEmpty() &&
        QRandomGenerator::global()->generateDouble() <= raceData.nicknameChance) {
        nickname = randomFrom(raceData.nicknames);
    }

    QString honorific;
    if (allowDecorativeTitles &&
        !raceData.honorifics.isEmpty() &&
        QRandomGenerator::global()->generateDouble() <= raceData.honorificChance) {
        honorific = randomFrom(raceData.honorifics);
    }

    const QString origin = randomFrom(raceData.origins);

    QStringList templates = raceData.formats;
    if (templates.isEmpty()) {
        templates = {
            QStringLiteral("{first} {surname}"),
            QStringLiteral("{first} {nickname}"),
            QStringLiteral("{first} \"{nickname}\" {surname}"),
            QStringLiteral("{first} of {origin}"),
            QStringLiteral("{first} {surname} {honorific}")
        };
    }

    if (genderKey == QStringLiteral("female")) {
        QStringList safeTemplates;
        for (const QString &templ : templates) {
            if (templ.contains(QStringLiteral("{nickname}")) || templ.contains(QStringLiteral("{honorific}"))) {
                continue;
            }
            safeTemplates << templ;
        }
        if (!safeTemplates.isEmpty()) {
            templates = safeTemplates;
        } else {
            templates = {QStringLiteral("{first} {surname}")};
        }
    }

    QStringList filteredTemplates;
    filteredTemplates.reserve(templates.size());
    for (const QString &templ : templates) {
        if (templ.contains(QStringLiteral("{surname}")) && surname.isEmpty()) {
            continue;
        }
        if (templ.contains(QStringLiteral("{nickname}")) && nickname.isEmpty()) {
            continue;
        }
        if (templ.contains(QStringLiteral("{honorific}")) && honorific.isEmpty()) {
            continue;
        }
        if (templ.contains(QStringLiteral("{origin}")) && origin.isEmpty()) {
            continue;
        }
        filteredTemplates << templ;
    }

    if (filteredTemplates.isEmpty()) {
        filteredTemplates = {
            QStringLiteral("{first} {surname}"),
            QStringLiteral("{first}")
        };
    }

    QString templateText = randomFrom(filteredTemplates);
    if (templateText.isEmpty()) {
        templateText = QStringLiteral("{first} {surname}");
    }

    QMap<QString, QString> tokens;
    tokens.insert(QStringLiteral("first"), first);
    tokens.insert(QStringLiteral("surname"), surname);
    tokens.insert(QStringLiteral("nickname"), nickname);
    tokens.insert(QStringLiteral("honorific"), honorific);
    tokens.insert(QStringLiteral("origin"), origin);

    QString result = applyTemplate(templateText, tokens);
    if (result.isEmpty()) {
        result = first;
    }
    return result;
}

QString NameGeneratorWidget::buildCompound(const PlaceNameData &data) const
{
    QString prefix = randomFrom(data.prefixes);
    QString core = randomFrom(data.cores);
    QString suffix = randomFrom(data.suffixes);

    QStringList parts;
    if (!prefix.isEmpty()) {
        parts << prefix;
    }
    if (!core.isEmpty() && QRandomGenerator::global()->bounded(100) < 60) {
        parts << core;
    }
    if (!suffix.isEmpty()) {
        parts << suffix;
    }

    QString compound = parts.join(QString());
    if (compound.isEmpty()) {
        compound = randomFrom(data.nouns);
    }
    return compound;
}

QString NameGeneratorWidget::generatePlaceName(const PlaceNameData &data) const
{
    QStringList templates = data.formats;
    if (templates.isEmpty()) {
        templates = {
            QStringLiteral("{compound}"),
            QStringLiteral("{adjective} {noun}"),
            QStringLiteral("{noun} {genitive}")
        };
    }

    const QString templ = randomFrom(templates);
    const QString noun = randomFrom(data.nouns);
    QMap<QString, QString> tokens;
    tokens.insert(QStringLiteral("compound"), buildCompound(data));
    tokens.insert(QStringLiteral("adjective"), randomFrom(data.adjectives));
    tokens.insert(QStringLiteral("noun"), noun);
    tokens.insert(QStringLiteral("nouns"), noun);
    tokens.insert(QStringLiteral("genitive"), randomFrom(data.genitives));

    QString result = applyTemplate(templ, tokens);
    if (result.isEmpty()) {
        result = buildCompound(data);
    }
    return result;
}

const NameGeneratorWidget::RaceNameData *NameGeneratorWidget::selectedRaceData() const
{
    const QString raceKey = raceCombo->currentData().toString();
    auto it = m_raceData.constFind(raceKey);
    if (it != m_raceData.constEnd()) {
        return &it.value();
    }
    it = m_raceData.constFind(m_fallbackRaceKey);
    if (it != m_raceData.constEnd()) {
        return &it.value();
    }
    return nullptr;
}

const NameGeneratorWidget::PlaceNameData *NameGeneratorWidget::selectedPlaceData() const
{
    const QString placeType = placeTypeCombo->currentData().toString();
    if (placeType == QStringLiteral("terrain")) {
        const QString terrainKey = terrainTypeCombo->currentData().toString();
        auto terrainIt = m_terrainData.constFind(terrainKey);
        if (terrainIt != m_terrainData.constEnd()) {
            return &terrainIt.value();
        }
        return m_terrainData.isEmpty() ? nullptr : &m_terrainData.first();
    }

    const auto settlementIt = m_settlementData.constFind(placeType);
    if (settlementIt != m_settlementData.constEnd()) {
        return &settlementIt.value();
    }
    return nullptr;
}

void NameGeneratorWidget::generateCharacterNames()
{
    const RaceNameData *raceData = selectedRaceData();
    if (!raceData) {
        namesOutput->setPlainText(QStringLiteral("Нет загруженных данных для генерации имён."));
        return;
    }

    const QString genderKey = genderCombo->currentData().toString();
    const int targetCount = nameCountSpin->value();

    QStringList generatedNames;
    QSet<QString> uniqueNames;

    int safetyCounter = qMax(200, targetCount * 40);
    while (generatedNames.size() < targetCount && safetyCounter-- > 0) {
        const QString candidate = generateCharacterName(*raceData, genderKey);
        if (candidate.isEmpty()) {
            continue;
        }
        if (uniqueNames.contains(candidate)) {
            continue;
        }
        uniqueNames.insert(candidate);
        generatedNames << candidate;
    }

    while (generatedNames.size() < targetCount) {
        const QString candidate = generateCharacterName(*raceData, genderKey);
        if (candidate.isEmpty()) {
            break;
        }
        generatedNames << candidate;
    }

    QStringList lines;
    lines.reserve(generatedNames.size());
    for (int i = 0; i < generatedNames.size(); ++i) {
        lines << QStringLiteral("%1. %2").arg(i + 1).arg(generatedNames.at(i));
    }
    namesOutput->setPlainText(lines.join(QLatin1Char('\n')));
}

void NameGeneratorWidget::generateLocationNames()
{
    const PlaceNameData *placeData = selectedPlaceData();
    if (!placeData) {
        placesOutput->setPlainText(QStringLiteral("Нет загруженных данных для генерации названий."));
        return;
    }

    const int targetCount = placeCountSpin->value();
    QStringList generatedNames;
    QSet<QString> uniqueNames;

    int safetyCounter = qMax(200, targetCount * 40);
    while (generatedNames.size() < targetCount && safetyCounter-- > 0) {
        const QString candidate = generatePlaceName(*placeData);
        if (candidate.isEmpty()) {
            continue;
        }
        if (uniqueNames.contains(candidate)) {
            continue;
        }
        uniqueNames.insert(candidate);
        generatedNames << candidate;
    }

    while (generatedNames.size() < targetCount) {
        const QString candidate = generatePlaceName(*placeData);
        if (candidate.isEmpty()) {
            break;
        }
        generatedNames << candidate;
    }

    QStringList lines;
    lines.reserve(generatedNames.size());
    for (int i = 0; i < generatedNames.size(); ++i) {
        lines << QStringLiteral("%1. %2").arg(i + 1).arg(generatedNames.at(i));
    }
    placesOutput->setPlainText(lines.join(QLatin1Char('\n')));
}

void NameGeneratorWidget::onPlaceTypeChanged()
{
    const bool isTerrain = placeTypeCombo->currentData().toString() == QStringLiteral("terrain");
    terrainTypeLabel->setVisible(isTerrain);
    terrainTypeCombo->setVisible(isTerrain);
}

