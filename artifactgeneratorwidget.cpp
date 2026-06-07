#include "artifactgeneratorwidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QtGlobal>
#include <algorithm>

namespace {

QStringList jsonStringList(const QJsonValue &value)
{
    QStringList values;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &entry : array) {
            const QString text = entry.toString().trimmed();
            if (!text.isEmpty()) {
                values << text;
            }
        }
    } else if (value.isString()) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            values << text;
        }
    }
    values.removeDuplicates();
    return values;
}

int jsonInt(const QJsonValue &value, int fallback, int minValue = -1000, int maxValue = 1000)
{
    int parsed = fallback;
    if (value.isDouble()) {
        parsed = value.toInt(fallback);
    } else if (value.isString()) {
        bool ok = false;
        const int stringValue = value.toString().trimmed().toInt(&ok);
        if (ok) {
            parsed = stringValue;
        }
    }
    return qBound(minValue, parsed, maxValue);
}

QString rarityRestoreDice(const QString &key)
{
    if (key == QStringLiteral("common")) {
        return QStringLiteral("1Рє4");
    }
    if (key == QStringLiteral("uncommon")) {
        return QStringLiteral("1Рє4");
    }
    if (key == QStringLiteral("rare")) {
        return QStringLiteral("1Рє4+1");
    }
    if (key == QStringLiteral("very_rare")) {
        return QStringLiteral("1Рє6+1");
    }
    if (key == QStringLiteral("legendary")) {
        return QStringLiteral("1Рє6+2");
    }
    return QStringLiteral("1Рє6+4");
}

} // namespace

ArtifactGeneratorWidget::ArtifactGeneratorWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ArtifactGeneratorWidget"));
    setupUi();

    if (!loadData()) {
        statusLabel->setText(QStringLiteral("Не удалось загрузить базу артефактов. Проверьте data/fantasy_artifacts.json."));
        statusLabel->setStyleSheet(QStringLiteral("color: #9d3a34;"));
    } else {
        statusLabel->setText(QStringLiteral("База артефактов загружена."));
        statusLabel->setStyleSheet(QStringLiteral("color: #3f6f4f;"));
    }

    fillCombos();

    connect(generateBtn, &QPushButton::clicked, this, &ArtifactGeneratorWidget::generateArtifacts);
    connect(generateRandomBtn, &QPushButton::clicked, this, &ArtifactGeneratorWidget::generateRandomArtifact);
    connect(strictArtifactCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        if (enabled) {
            const int artifactIndex = rarityCombo->findData(QStringLiteral("artifact"));
            if (artifactIndex >= 0) {
                rarityCombo->setCurrentIndex(artifactIndex);
            }
        }
        rarityCombo->setEnabled(!enabled);
        drawbacksCheck->setChecked(enabled ? true : drawbacksCheck->isChecked());
    });
}

void ArtifactGeneratorWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(12);

    QGroupBox *paramsGroup = new QGroupBox(QStringLiteral("Параметры генерации артефактов"), this);
    QVBoxLayout *paramsLayout = new QVBoxLayout(paramsGroup);

    QFormLayout *form = new QFormLayout();

    typeCombo = new QComboBox(paramsGroup);
    rarityCombo = new QComboBox(paramsGroup);
    themeCombo = new QComboBox(paramsGroup);
    purposeCombo = new QComboBox(paramsGroup);

    attunementModeCombo = new QComboBox(paramsGroup);
    attunementModeCombo->addItem(QStringLiteral("Авто (по редкости)"), QStringLiteral("auto"));
    attunementModeCombo->addItem(QStringLiteral("Всегда требуется настройка"), QStringLiteral("required"));
    attunementModeCombo->addItem(QStringLiteral("Без настройки"), QStringLiteral("none"));

    countSpin = new QSpinBox(paramsGroup);
    countSpin->setRange(1, 50);
    countSpin->setValue(5);

    form->addRow(QStringLiteral("Тип артефакта:"), typeCombo);
    form->addRow(QStringLiteral("Редкость:"), rarityCombo);
    form->addRow(QStringLiteral("Тема:"), themeCombo);
    form->addRow(QStringLiteral("Назначение:"), purposeCombo);
    form->addRow(QStringLiteral("Настройка:"), attunementModeCombo);
    form->addRow(QStringLiteral("Количество:"), countSpin);
    paramsLayout->addLayout(form);

    drawbacksCheck = new QCheckBox(QStringLiteral("Добавлять вредные/проклятые свойства"), paramsGroup);
    drawbacksCheck->setChecked(true);

    strictArtifactCheck = new QCheckBox(QStringLiteral("Строгий режим артефакта (по правилам DMG)"), paramsGroup);
    strictArtifactCheck->setChecked(true);

    paramsLayout->addWidget(drawbacksCheck);
    paramsLayout->addWidget(strictArtifactCheck);

    QHBoxLayout *buttonRow = new QHBoxLayout();
    generateBtn = new QPushButton(QStringLiteral("Сгенерировать артефакты"), paramsGroup);
    generateBtn->setProperty("variant", QStringLiteral("accent"));
    generateRandomBtn = new QPushButton(QStringLiteral("Полностью случайный артефакт"), paramsGroup);
    generateRandomBtn->setProperty("variant", QStringLiteral("accent"));
    buttonRow->addWidget(generateBtn, 1);
    buttonRow->addWidget(generateRandomBtn, 1);
    paramsLayout->addLayout(buttonRow);

    resultOutput = new QPlainTextEdit(this);
    resultOutput->setReadOnly(true);
    resultOutput->setPlaceholderText(
        QStringLiteral("Здесь появятся сгенерированные артефакты: название, редкость, свойства, цена, история и условия уничтожения."));
    resultOutput->setMinimumHeight(180);

    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);

    mainLayout->addWidget(paramsGroup);
    mainLayout->addWidget(resultOutput, 1);
    mainLayout->addWidget(statusLabel);
}
QString ArtifactGeneratorWidget::resolveDataPath(const QString &relativePath) const
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

bool ArtifactGeneratorWidget::loadData()
{
    m_rarities.clear();
    m_types.clear();
    m_themes.clear();
    m_purposes.clear();
    m_minorBeneficial.clear();
    m_majorBeneficial.clear();
    m_minorDetrimental.clear();
    m_majorDetrimental.clear();
    m_chargeRules.clear();
    m_activationWords.clear();
    m_historyTemplates.clear();
    m_appearanceTemplates.clear();
    m_defaultRarityKey.clear();

    const QString dataPath = resolveDataPath(QStringLiteral("data/fantasy_artifacts.json"));
    if (dataPath.isEmpty()) {
        return false;
    }
    return loadArtifactData(dataPath);
}

bool ArtifactGeneratorWidget::loadArtifactData(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    m_defaultRarityKey = root.value(QStringLiteral("defaultRarity")).toString(QStringLiteral("rare"));

    const QJsonArray rarities = root.value(QStringLiteral("rarities")).toArray();
    for (const QJsonValue &value : rarities) {
        const QJsonObject obj = value.toObject();
        if (obj.isEmpty()) {
            continue;
        }

        RarityRule rule;
        rule.key = obj.value(QStringLiteral("key")).toString().trimmed();
        rule.display = obj.value(QStringLiteral("display")).toString(rule.key);
        rule.price = obj.value(QStringLiteral("price")).toString(QStringLiteral("—"));
        rule.attunementChance = jsonInt(obj.value(QStringLiteral("attunementChance")), rule.attunementChance, 0, 100);
        rule.bonusMin = jsonInt(obj.value(QStringLiteral("bonusMin")), rule.bonusMin, 0, 10);
        rule.bonusMax = jsonInt(obj.value(QStringLiteral("bonusMax")), rule.bonusMax, 0, 10);
        rule.passiveMin = jsonInt(obj.value(QStringLiteral("passiveMin")), rule.passiveMin, 0, 10);
        rule.passiveMax = jsonInt(obj.value(QStringLiteral("passiveMax")), rule.passiveMax, 0, 10);
        rule.activeMin = jsonInt(obj.value(QStringLiteral("activeMin")), rule.activeMin, 0, 10);
        rule.activeMax = jsonInt(obj.value(QStringLiteral("activeMax")), rule.activeMax, 0, 10);
        rule.minorBeneficialMin = jsonInt(obj.value(QStringLiteral("minorBeneficialMin")), rule.minorBeneficialMin, 0, 10);
        rule.minorBeneficialMax = jsonInt(obj.value(QStringLiteral("minorBeneficialMax")), rule.minorBeneficialMax, 0, 10);
        rule.majorBeneficialMin = jsonInt(obj.value(QStringLiteral("majorBeneficialMin")), rule.majorBeneficialMin, 0, 10);
        rule.majorBeneficialMax = jsonInt(obj.value(QStringLiteral("majorBeneficialMax")), rule.majorBeneficialMax, 0, 10);
        rule.minorDetrimentalMin = jsonInt(obj.value(QStringLiteral("minorDetrimentalMin")), rule.minorDetrimentalMin, 0, 10);
        rule.minorDetrimentalMax = jsonInt(obj.value(QStringLiteral("minorDetrimentalMax")), rule.minorDetrimentalMax, 0, 10);
        rule.majorDetrimentalMin = jsonInt(obj.value(QStringLiteral("majorDetrimentalMin")), rule.majorDetrimentalMin, 0, 10);
        rule.majorDetrimentalMax = jsonInt(obj.value(QStringLiteral("majorDetrimentalMax")), rule.majorDetrimentalMax, 0, 10);
        rule.chargeChance = jsonInt(obj.value(QStringLiteral("chargeChance")), rule.chargeChance, 0, 100);

        if (rule.key.isEmpty()) {
            continue;
        }

        if (rule.bonusMin > rule.bonusMax) {
            std::swap(rule.bonusMin, rule.bonusMax);
        }
        if (rule.passiveMin > rule.passiveMax) {
            std::swap(rule.passiveMin, rule.passiveMax);
        }
        if (rule.activeMin > rule.activeMax) {
            std::swap(rule.activeMin, rule.activeMax);
        }
        if (rule.minorBeneficialMin > rule.minorBeneficialMax) {
            std::swap(rule.minorBeneficialMin, rule.minorBeneficialMax);
        }
        if (rule.majorBeneficialMin > rule.majorBeneficialMax) {
            std::swap(rule.majorBeneficialMin, rule.majorBeneficialMax);
        }
        if (rule.minorDetrimentalMin > rule.minorDetrimentalMax) {
            std::swap(rule.minorDetrimentalMin, rule.minorDetrimentalMax);
        }
        if (rule.majorDetrimentalMin > rule.majorDetrimentalMax) {
            std::swap(rule.majorDetrimentalMin, rule.majorDetrimentalMax);
        }

        m_rarities.insert(rule.key, rule);
    }

    const QJsonObject typesObj = root.value(QStringLiteral("types")).toObject();
    for (auto it = typesObj.begin(); it != typesObj.end(); ++it) {
        const QString key = it.key().trimmed();
        const QJsonObject obj = it.value().toObject();
        if (key.isEmpty() || obj.isEmpty()) {
            continue;
        }

        ArtifactTypeData type;
        type.key = key;
        type.display = obj.value(QStringLiteral("display")).toString(key);
        type.nameCores = jsonStringList(obj.value(QStringLiteral("nameCores")));
        type.nameSuffixes = jsonStringList(obj.value(QStringLiteral("nameSuffixes")));
        type.nameFormats = jsonStringList(obj.value(QStringLiteral("nameFormats")));
        type.forms = jsonStringList(obj.value(QStringLiteral("forms")));
        type.passivePool = jsonStringList(obj.value(QStringLiteral("passivePool")));
        type.activePool = jsonStringList(obj.value(QStringLiteral("activePool")));
        type.attunementRequirements = jsonStringList(obj.value(QStringLiteral("attunementRequirements")));

        m_types.insert(type.key, type);
    }

    const QJsonObject themesObj = root.value(QStringLiteral("themes")).toObject();
    for (auto it = themesObj.begin(); it != themesObj.end(); ++it) {
        const QString key = it.key().trimmed();
        const QJsonObject obj = it.value().toObject();
        if (key.isEmpty() || obj.isEmpty()) {
            continue;
        }

        ThemeData theme;
        theme.key = key;
        theme.display = obj.value(QStringLiteral("display")).toString(key);
        theme.prefixes = jsonStringList(obj.value(QStringLiteral("prefixes")));
        theme.origins = jsonStringList(obj.value(QStringLiteral("origins")));
        theme.materials = jsonStringList(obj.value(QStringLiteral("materials")));
        theme.passivePool = jsonStringList(obj.value(QStringLiteral("passivePool")));
        theme.activePool = jsonStringList(obj.value(QStringLiteral("activePool")));
        theme.curses = jsonStringList(obj.value(QStringLiteral("curses")));
        theme.quirks = jsonStringList(obj.value(QStringLiteral("quirks")));
        theme.destroy = jsonStringList(obj.value(QStringLiteral("destroy")));

        m_themes.insert(theme.key, theme);
    }

    const QJsonObject purposesObj = root.value(QStringLiteral("purposes")).toObject();
    for (auto it = purposesObj.begin(); it != purposesObj.end(); ++it) {
        const QString key = it.key().trimmed();
        const QJsonObject obj = it.value().toObject();
        if (key.isEmpty() || obj.isEmpty()) {
            continue;
        }

        PurposeData purpose;
        purpose.key = key;
        purpose.display = obj.value(QStringLiteral("display")).toString(key);
        purpose.passivePool = jsonStringList(obj.value(QStringLiteral("passivePool")));
        purpose.activePool = jsonStringList(obj.value(QStringLiteral("activePool")));

        m_purposes.insert(purpose.key, purpose);
    }

    m_minorBeneficial = jsonStringList(root.value(QStringLiteral("minorBeneficial")));
    m_majorBeneficial = jsonStringList(root.value(QStringLiteral("majorBeneficial")));
    m_minorDetrimental = jsonStringList(root.value(QStringLiteral("minorDetrimental")));
    m_majorDetrimental = jsonStringList(root.value(QStringLiteral("majorDetrimental")));
    m_chargeRules = jsonStringList(root.value(QStringLiteral("chargeRules")));
    m_activationWords = jsonStringList(root.value(QStringLiteral("activationWords")));
    m_historyTemplates = jsonStringList(root.value(QStringLiteral("historyTemplates")));
    m_appearanceTemplates = jsonStringList(root.value(QStringLiteral("appearanceTemplates")));

    return !m_rarities.isEmpty() && !m_types.isEmpty() && !m_themes.isEmpty() && !m_purposes.isEmpty();
}

void ArtifactGeneratorWidget::fillCombos()
{
    typeCombo->clear();
    rarityCombo->clear();
    themeCombo->clear();
    purposeCombo->clear();

    struct DisplayEntry {
        QString key;
        QString display;
    };

    QList<DisplayEntry> typeEntries;
    for (auto it = m_types.constBegin(); it != m_types.constEnd(); ++it) {
        typeEntries.append({it.key(), it.value().display});
    }
    std::sort(typeEntries.begin(), typeEntries.end(), [](const DisplayEntry &left, const DisplayEntry &right) {
        return left.display.localeAwareCompare(right.display) < 0;
    });
    for (const DisplayEntry &entry : typeEntries) {
        typeCombo->addItem(entry.display, entry.key);
    }

    QList<DisplayEntry> rarityEntries;
    for (auto it = m_rarities.constBegin(); it != m_rarities.constEnd(); ++it) {
        rarityEntries.append({it.key(), it.value().display});
    }
    std::sort(rarityEntries.begin(), rarityEntries.end(), [](const DisplayEntry &left, const DisplayEntry &right) {
        return left.display.localeAwareCompare(right.display) < 0;
    });
    for (const DisplayEntry &entry : rarityEntries) {
        rarityCombo->addItem(entry.display, entry.key);
    }

    QList<DisplayEntry> themeEntries;
    for (auto it = m_themes.constBegin(); it != m_themes.constEnd(); ++it) {
        themeEntries.append({it.key(), it.value().display});
    }
    std::sort(themeEntries.begin(), themeEntries.end(), [](const DisplayEntry &left, const DisplayEntry &right) {
        return left.display.localeAwareCompare(right.display) < 0;
    });
    for (const DisplayEntry &entry : themeEntries) {
        themeCombo->addItem(entry.display, entry.key);
    }

    QList<DisplayEntry> purposeEntries;
    for (auto it = m_purposes.constBegin(); it != m_purposes.constEnd(); ++it) {
        purposeEntries.append({it.key(), it.value().display});
    }
    std::sort(purposeEntries.begin(), purposeEntries.end(), [](const DisplayEntry &left, const DisplayEntry &right) {
        return left.display.localeAwareCompare(right.display) < 0;
    });
    for (const DisplayEntry &entry : purposeEntries) {
        purposeCombo->addItem(entry.display, entry.key);
    }

    const int defaultRarityIndex = rarityCombo->findData(m_defaultRarityKey);
    if (defaultRarityIndex >= 0) {
        rarityCombo->setCurrentIndex(defaultRarityIndex);
    }

    const int artifactIndex = rarityCombo->findData(QStringLiteral("artifact"));
    if (artifactIndex >= 0 && strictArtifactCheck && strictArtifactCheck->isChecked()) {
        rarityCombo->setCurrentIndex(artifactIndex);
    }
}

QString ArtifactGeneratorWidget::randomFrom(const QStringList &values) const
{
    if (values.isEmpty()) {
        return QString();
    }
    const int index = QRandomGenerator::global()->bounded(values.size());
    return values.at(index);
}

int ArtifactGeneratorWidget::rollInclusive(int minValue, int maxValue) const
{
    if (minValue > maxValue) {
        std::swap(minValue, maxValue);
    }
    if (minValue == maxValue) {
        return minValue;
    }
    return QRandomGenerator::global()->bounded(maxValue - minValue + 1) + minValue;
}

QString ArtifactGeneratorWidget::cleanupText(QString text) const
{
    text.replace(QRegularExpression(QStringLiteral("\\{[^}]+\\}")), QString());
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral("\\s+,\\s*")), QStringLiteral(", "));
    text.replace(QRegularExpression(QStringLiteral("\\s+\\.\\s*")), QStringLiteral(". "));
    text.replace(QRegularExpression(QStringLiteral("\\(\\s+")), QStringLiteral("("));
    text.replace(QRegularExpression(QStringLiteral("\\s+\\)")), QStringLiteral(")"));
    return text.trimmed();
}

QString ArtifactGeneratorWidget::applyTemplate(QString templ, const QMap<QString, QString> &tokens) const
{
    if (templ.trimmed().isEmpty()) {
        templ = QStringLiteral("{prefix} {core}{suffix}");
    }
    for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
        templ.replace(QStringLiteral("{%1}").arg(it.key()), it.value());
    }
    return cleanupText(templ);
}

QString ArtifactGeneratorWidget::chooseAttunement(const ArtifactTypeData &typeData,
                                                  const RarityRule &rarityRule,
                                                  const QString &modeOverride) const
{
    const QString mode = modeOverride.isEmpty() ? attunementModeCombo->currentData().toString() : modeOverride;

    bool requiresAttunement = false;
    if (mode == QStringLiteral("required")) {
        requiresAttunement = true;
    } else if (mode == QStringLiteral("none")) {
        requiresAttunement = false;
    } else {
        int chance = rarityRule.attunementChance;
        if (strictArtifactCheck->isChecked()) {
            chance = qMax(chance, 80);
        }
        requiresAttunement = QRandomGenerator::global()->bounded(100) < chance;
    }

    if (!requiresAttunement) {
        return QStringLiteral("Не требуется");
    }

    const QString requirement = randomFrom(typeData.attunementRequirements);
    if (requirement.isEmpty()) {
        return QStringLiteral("Требуется настройка");
    }
    return QStringLiteral("Требуется настройка (%1)").arg(requirement);
}

QString ArtifactGeneratorWidget::buildName(const ArtifactTypeData &typeData, const ThemeData &themeData) const
{
    QStringList templates = typeData.nameFormats;
    if (templates.isEmpty()) {
        templates = {
            QStringLiteral("{prefix} {core}{suffix}"),
            QStringLiteral("{core}{suffix} {theme}"),
            QStringLiteral("{type} В«{prefix} {core}В»"),
            QStringLiteral("{prefix} {type} {suffix}")
        };
    }

    QMap<QString, QString> tokens;
    tokens.insert(QStringLiteral("prefix"), randomFrom(themeData.prefixes));
    tokens.insert(QStringLiteral("core"), randomFrom(typeData.nameCores));
    tokens.insert(QStringLiteral("suffix"), randomFrom(typeData.nameSuffixes));
    tokens.insert(QStringLiteral("type"), typeData.display);
    tokens.insert(QStringLiteral("theme"), themeData.display.toLower());
    tokens.insert(QStringLiteral("origin"), randomFrom(themeData.origins));
    tokens.insert(QStringLiteral("material"), randomFrom(themeData.materials));
    tokens.insert(QStringLiteral("form"), randomFrom(typeData.forms).toLower());

    QString name = applyTemplate(randomFrom(templates), tokens);

    if (name.isEmpty()) {
        name = cleanupText(tokens.value(QStringLiteral("prefix")) + QLatin1Char(' ') +
                           tokens.value(QStringLiteral("core")) + tokens.value(QStringLiteral("suffix")));
    }
    if (name.isEmpty()) {
        name = QStringLiteral("%1 %2").arg(typeData.display, themeData.display.toLower());
    }
    if (!name.isEmpty()) {
        name[0] = name.at(0).toUpper();
    }
    return name;
}

QString ArtifactGeneratorWidget::buildChargesText(const RarityRule &rarityRule) const
{
    if (m_chargeRules.isEmpty()) {
        return QString();
    }

    int chance = rarityRule.chargeChance;
    if (strictArtifactCheck->isChecked() && rarityRule.key == QStringLiteral("artifact")) {
        chance = qMax(chance, 95);
    }
    if (QRandomGenerator::global()->bounded(100) >= chance) {
        return QString();
    }

    int minCharges = 2;
    int maxCharges = 4;
    if (rarityRule.key == QStringLiteral("common")) {
        minCharges = 1;
        maxCharges = 3;
    } else if (rarityRule.key == QStringLiteral("uncommon")) {
        minCharges = 2;
        maxCharges = 4;
    } else if (rarityRule.key == QStringLiteral("rare")) {
        minCharges = 3;
        maxCharges = 6;
    } else if (rarityRule.key == QStringLiteral("very_rare")) {
        minCharges = 4;
        maxCharges = 8;
    } else if (rarityRule.key == QStringLiteral("legendary")) {
        minCharges = 5;
        maxCharges = 10;
    } else if (rarityRule.key == QStringLiteral("artifact")) {
        minCharges = 6;
        maxCharges = 12;
    }

    const int charges = rollInclusive(minCharges, maxCharges);

    QString restore = rarityRestoreDice(rarityRule.key);
    if (rarityRule.key == QStringLiteral("artifact")) {
        const QStringList artifactRestore = {QStringLiteral("1Рє6+4"), QStringLiteral("2Рє6+2"), QStringLiteral("РІСЃРµ")};
        restore = randomFrom(artifactRestore);
    }

    QMap<QString, QString> tokens;
    tokens.insert(QStringLiteral("charges"), QString::number(charges));
    tokens.insert(QStringLiteral("restore"), restore);
    tokens.insert(QStringLiteral("time"), QStringLiteral("на рассвете"));

    QString text = applyTemplate(randomFrom(m_chargeRules), tokens);
    if (!text.endsWith(QLatin1Char('.'))) {
        text.append(QLatin1Char('.'));
    }
    return text;
}

QString ArtifactGeneratorWidget::buildAppearance(const ArtifactTypeData &typeData, const ThemeData &themeData) const
{
    QStringList templates = m_appearanceTemplates;
    if (templates.isEmpty()) {
        templates = {
            QStringLiteral("Выполнен из {material}; на поверхности заметны руны и знаки темы «{theme}»."),
            QStringLiteral("{form} с отделкой из {material} и узором, напоминающим происхождение «{origin}»."),
            QStringLiteral("С виду это {form}, но при магическом анализе проявляются символы {theme}.")
        };
    }

    const QStringList colors = {
        QStringLiteral("сапфировый"),
        QStringLiteral("янтарный"),
        QStringLiteral("пепельный"),
        QStringLiteral("изумрудный"),
        QStringLiteral("обсидиановый"),
        QStringLiteral("серебристый")
    };

    QMap<QString, QString> tokens;
    tokens.insert(QStringLiteral("material"), randomFrom(themeData.materials));
    tokens.insert(QStringLiteral("theme"), themeData.display.toLower());
    tokens.insert(QStringLiteral("origin"), randomFrom(themeData.origins));
    tokens.insert(QStringLiteral("form"), randomFrom(typeData.forms).toLower());
    tokens.insert(QStringLiteral("color"), randomFrom(colors));

    return applyTemplate(randomFrom(templates), tokens);
}

QString ArtifactGeneratorWidget::buildHistory(const ArtifactTypeData &typeData, const ThemeData &themeData) const
{
    QStringList templates = m_historyTemplates;
    if (templates.isEmpty()) {
        templates = {
            QStringLiteral("Создан в эпоху катаклизмов мастерами из {origin} для носителя, связанного с темой «{theme}»."),
            QStringLiteral("По легендам этот {typeLower} потерян после падения ордена в {origin}."),
            QStringLiteral("Артефакт пережил несколько войн; последний раз его видели на пути к {origin}.")
        };
    }

    const QStringList epochs = {
        QStringLiteral("Эпоху Драконов"),
        QStringLiteral("Время Теневых Войн"),
        QStringLiteral("Эру Раскола"),
        QStringLiteral("Период Семи Королей")
    };
    const QStringList creators = {
        QStringLiteral("архимагами"),
        QStringLiteral("жрецами"),
        QStringLiteral("руническими кузнецами"),
        QStringLiteral("друидами"),
        QStringLiteral("безымянным орденом")
    };

    QMap<QString, QString> tokens;
    tokens.insert(QStringLiteral("origin"), randomFrom(themeData.origins));
    tokens.insert(QStringLiteral("theme"), themeData.display.toLower());
    tokens.insert(QStringLiteral("type"), typeData.display);
    tokens.insert(QStringLiteral("typeLower"), typeData.display.toLower());
    tokens.insert(QStringLiteral("epoch"), randomFrom(epochs));
    tokens.insert(QStringLiteral("creator"), randomFrom(creators));

    return applyTemplate(randomFrom(templates), tokens);
}

QString ArtifactGeneratorWidget::buildDestruction(const ThemeData &themeData) const
{
    if (!themeData.destroy.isEmpty()) {
        return randomFrom(themeData.destroy);
    }

    const QStringList fallbackDestroy = {
        QStringLiteral("Уничтожается только в месте своего создания после ритуала на 24 часа."),
        QStringLiteral("Можно разрушить, погрузив в первозданный источник магии противоположной школы."),
        QStringLiteral("Теряет силу и рассыпается, если добровольно передан смертельному врагу владельца.")
    };
    return randomFrom(fallbackDestroy);
}

QString ArtifactGeneratorWidget::renderArtifact(const ArtifactResult &result, int index) const
{
    QStringList lines;
    lines << QStringLiteral("%1. %2").arg(index).arg(result.name);
    lines << QStringLiteral("РўРёРї: %1 (%2)").arg(result.typeDisplay, result.form);
    lines << QStringLiteral("Редкость: %1").arg(result.rarityDisplay);
    lines << QStringLiteral("Стоимость: %1").arg(result.price);
    lines << QStringLiteral("Настройка: %1").arg(result.attunement);

    if (!result.appearance.isEmpty()) {
        lines << QStringLiteral("Внешний вид: %1").arg(result.appearance);
    }
    if (!result.history.isEmpty()) {
        lines << QStringLiteral("История: %1").arg(result.history);
    }

    if (!result.passives.isEmpty()) {
        lines << QStringLiteral("Постоянные свойства:");
        for (const QString &entry : result.passives) {
            lines << QStringLiteral("- %1").arg(entry);
        }
    }

    if (!result.actives.isEmpty()) {
        lines << QStringLiteral("Активируемые свойства:");
        for (const QString &entry : result.actives) {
            lines << QStringLiteral("- %1").arg(entry);
        }
    }

    if (!result.charges.isEmpty()) {
        lines << QStringLiteral("Заряды: %1").arg(result.charges);
    }
    if (!result.activationWord.isEmpty()) {
        lines << QStringLiteral("Активация: %1").arg(result.activationWord);
    }

    if (!result.minorBeneficial.isEmpty()) {
        lines << QStringLiteral("Минорные полезные свойства:");
        for (const QString &entry : result.minorBeneficial) {
            lines << QStringLiteral("- %1").arg(entry);
        }
    }

    if (!result.majorBeneficial.isEmpty()) {
        lines << QStringLiteral("Мажорные полезные свойства:");
        for (const QString &entry : result.majorBeneficial) {
            lines << QStringLiteral("- %1").arg(entry);
        }
    }

    if (!result.minorDetrimental.isEmpty()) {
        lines << QStringLiteral("Минорные вредные свойства:");
        for (const QString &entry : result.minorDetrimental) {
            lines << QStringLiteral("- %1").arg(entry);
        }
    }

    if (!result.majorDetrimental.isEmpty()) {
        lines << QStringLiteral("Мажорные вредные свойства:");
        for (const QString &entry : result.majorDetrimental) {
            lines << QStringLiteral("- %1").arg(entry);
        }
    }

    if (!result.quirks.isEmpty()) {
        lines << QStringLiteral("Причуды артефакта:");
        for (const QString &entry : result.quirks) {
            lines << QStringLiteral("- %1").arg(entry);
        }
    }

    if (!result.destruction.isEmpty()) {
        lines << QStringLiteral("Условия уничтожения: %1").arg(result.destruction);
    }

    return lines.join(QLatin1Char('\n'));
}

QStringList ArtifactGeneratorWidget::takeUniqueRandom(const QStringList &pool, int count) const
{
    if (pool.isEmpty() || count <= 0) {
        return {};
    }

    QStringList source = pool;
    QStringList result;
    result.reserve(count);

    while (result.size() < count && !source.isEmpty()) {
        const int index = QRandomGenerator::global()->bounded(source.size());
        result << source.takeAt(index);
    }

    while (result.size() < count && !pool.isEmpty()) {
        result << randomFrom(pool);
    }

    return result;
}

ArtifactGeneratorWidget::ArtifactResult ArtifactGeneratorWidget::generateOneArtifact(bool fullyRandom) const
{
    ArtifactResult result;

    const ArtifactTypeData *typeData = fullyRandom ? randomType() : selectedType();
    const ThemeData *themeData = fullyRandom ? randomTheme() : selectedTheme();
    const PurposeData *purposeData = fullyRandom ? randomPurpose() : selectedPurpose();
    const RarityRule *rarityRule = fullyRandom ? randomRarity() : selectedRarity();

    if (!typeData || !themeData || !purposeData || !rarityRule) {
        result.name = QStringLiteral("Ошибка генерации");
        return result;
    }

    bool strictMode = strictArtifactCheck->isChecked();
    if (fullyRandom) {
        strictMode = QRandomGenerator::global()->bounded(100) < 45;
    }
    if (strictMode) {
        auto artifactIt = m_rarities.constFind(QStringLiteral("artifact"));
        if (artifactIt != m_rarities.constEnd()) {
            rarityRule = &artifactIt.value();
        }
    }

    result.rarityDisplay = rarityRule->display;
    result.typeDisplay = typeData->display;
    result.form = randomFrom(typeData->forms);
    if (result.form.isEmpty()) {
        result.form = typeData->display;
    }
    result.name = buildName(*typeData, *themeData);
    QString attunementModeOverride;
    if (fullyRandom) {
        const QStringList modes = {QStringLiteral("auto"), QStringLiteral("required"), QStringLiteral("none")};
        attunementModeOverride = randomFrom(modes);
    }
    result.attunement = chooseAttunement(*typeData, *rarityRule, attunementModeOverride);
    result.price = rarityRule->price;

    result.appearance = buildAppearance(*typeData, *themeData);
    result.history = buildHistory(*typeData, *themeData);
    result.destruction = buildDestruction(*themeData);

    QStringList passivePool = typeData->passivePool;
    passivePool << themeData->passivePool << purposeData->passivePool;
    passivePool.removeDuplicates();

    QStringList activePool = typeData->activePool;
    activePool << themeData->activePool << purposeData->activePool;
    activePool.removeDuplicates();

    if (passivePool.isEmpty()) {
        passivePool << QStringLiteral("Даёт преимущество на одну тематическую проверку раз в короткий отдых.");
    }
    if (activePool.isEmpty()) {
        activePool << QStringLiteral("Действием выпускает направленный магический импульс (СЛ 15).");
    }

    int passiveCount = rollInclusive(rarityRule->passiveMin, rarityRule->passiveMax);
    int activeCount = rollInclusive(rarityRule->activeMin, rarityRule->activeMax);
    if (strictMode) {
        passiveCount = qMax(passiveCount, 2);
        activeCount = qMax(activeCount, 1);
    }

    result.passives = takeUniqueRandom(passivePool, passiveCount);
    result.actives = takeUniqueRandom(activePool, activeCount);

    if (rarityRule->bonusMax > 0) {
        const int bonus = rollInclusive(qMax(0, rarityRule->bonusMin), qMax(0, rarityRule->bonusMax));
        if (bonus > 0) {
            QString bonusText;
            if (typeData->key == QStringLiteral("weapon")) {
                bonusText = QStringLiteral("+%1 к броскам атаки и урона этим оружием.").arg(bonus);
            } else if (typeData->key == QStringLiteral("armor") || typeData->key == QStringLiteral("shield")) {
                bonusText = QStringLiteral("+%1 к КД при использовании предмета.").arg(bonus);
            } else if (typeData->key == QStringLiteral("wand") || typeData->key == QStringLiteral("staff")) {
                bonusText = QStringLiteral("+%1 к СЛ спасброска ваших заклинаний и броскам атаки заклинаниями.").arg(bonus);
            } else if (typeData->key == QStringLiteral("tome")) {
                bonusText = QStringLiteral("+%1 к проверкам Интеллекта, связанным с тематикой артефакта.").arg(bonus);
            } else {
                bonusText = QStringLiteral("+%1 к выбранной проверке характеристики владельца (на усмотрение мастера).").arg(bonus);
            }
            result.passives.prepend(bonusText);
        }
    }

    int minorBenCount = rollInclusive(rarityRule->minorBeneficialMin, rarityRule->minorBeneficialMax);
    int majorBenCount = rollInclusive(rarityRule->majorBeneficialMin, rarityRule->majorBeneficialMax);
    if (strictMode && rarityRule->key == QStringLiteral("artifact")) {
        minorBenCount = qMax(minorBenCount, 1);
        majorBenCount = qMax(majorBenCount, 1);
    }

    result.minorBeneficial = takeUniqueRandom(m_minorBeneficial, minorBenCount);
    result.majorBeneficial = takeUniqueRandom(m_majorBeneficial, majorBenCount);

    bool includeDetrimental = drawbacksCheck->isChecked();
    if (fullyRandom) {
        includeDetrimental = QRandomGenerator::global()->bounded(100) < 70;
    }
    if (strictMode) {
        includeDetrimental = true;
    }

    if (includeDetrimental) {
        QStringList minorDetrimentalPool = m_minorDetrimental;
        minorDetrimentalPool << themeData->curses;
        minorDetrimentalPool.removeDuplicates();

        QStringList majorDetrimentalPool = m_majorDetrimental;
        majorDetrimentalPool << themeData->curses;
        majorDetrimentalPool.removeDuplicates();

        int minorDetrimentalCount = rollInclusive(rarityRule->minorDetrimentalMin, rarityRule->minorDetrimentalMax);
        int majorDetrimentalCount = rollInclusive(rarityRule->majorDetrimentalMin, rarityRule->majorDetrimentalMax);
        if (strictMode && rarityRule->key == QStringLiteral("artifact")) {
            minorDetrimentalCount = qMax(minorDetrimentalCount, 1);
        }

        result.minorDetrimental = takeUniqueRandom(minorDetrimentalPool, minorDetrimentalCount);
        result.majorDetrimental = takeUniqueRandom(majorDetrimentalPool, majorDetrimentalCount);
    }

    int quirkCount = 0;
    if (strictMode) {
        quirkCount = rollInclusive(1, 2);
    } else if (!themeData->quirks.isEmpty() && QRandomGenerator::global()->bounded(100) < 45) {
        quirkCount = 1;
    }
    result.quirks = takeUniqueRandom(themeData->quirks, quirkCount);

    result.charges = buildChargesText(*rarityRule);
    if (!result.actives.isEmpty()) {
        const QString word = randomFrom(m_activationWords);
        if (!word.isEmpty()) {
            result.activationWord = QStringLiteral("Ключевая фраза: «%1»").arg(word);
        }
    }

    return result;
}

const ArtifactGeneratorWidget::ArtifactTypeData *ArtifactGeneratorWidget::selectedType() const
{
    if (m_types.isEmpty()) {
        return nullptr;
    }

    const QString key = typeCombo->currentData().toString();
    auto it = m_types.constFind(key);
    if (it != m_types.constEnd()) {
        return &it.value();
    }
    return &m_types.first();
}

const ArtifactGeneratorWidget::ThemeData *ArtifactGeneratorWidget::selectedTheme() const
{
    if (m_themes.isEmpty()) {
        return nullptr;
    }

    const QString key = themeCombo->currentData().toString();
    auto it = m_themes.constFind(key);
    if (it != m_themes.constEnd()) {
        return &it.value();
    }
    return &m_themes.first();
}

const ArtifactGeneratorWidget::PurposeData *ArtifactGeneratorWidget::selectedPurpose() const
{
    if (m_purposes.isEmpty()) {
        return nullptr;
    }

    const QString key = purposeCombo->currentData().toString();
    auto it = m_purposes.constFind(key);
    if (it != m_purposes.constEnd()) {
        return &it.value();
    }
    return &m_purposes.first();
}

const ArtifactGeneratorWidget::RarityRule *ArtifactGeneratorWidget::selectedRarity() const
{
    if (m_rarities.isEmpty()) {
        return nullptr;
    }

    const QString key = rarityCombo->currentData().toString();
    auto it = m_rarities.constFind(key);
    if (it != m_rarities.constEnd()) {
        return &it.value();
    }

    it = m_rarities.constFind(m_defaultRarityKey);
    if (it != m_rarities.constEnd()) {
        return &it.value();
    }

    return &m_rarities.first();
}

const ArtifactGeneratorWidget::ArtifactTypeData *ArtifactGeneratorWidget::randomType() const
{
    if (m_types.isEmpty()) {
        return nullptr;
    }
    const QStringList keys = m_types.keys();
    const QString key = keys.at(QRandomGenerator::global()->bounded(keys.size()));
    auto it = m_types.constFind(key);
    return it == m_types.constEnd() ? nullptr : &it.value();
}

const ArtifactGeneratorWidget::ThemeData *ArtifactGeneratorWidget::randomTheme() const
{
    if (m_themes.isEmpty()) {
        return nullptr;
    }
    const QStringList keys = m_themes.keys();
    const QString key = keys.at(QRandomGenerator::global()->bounded(keys.size()));
    auto it = m_themes.constFind(key);
    return it == m_themes.constEnd() ? nullptr : &it.value();
}

const ArtifactGeneratorWidget::PurposeData *ArtifactGeneratorWidget::randomPurpose() const
{
    if (m_purposes.isEmpty()) {
        return nullptr;
    }
    const QStringList keys = m_purposes.keys();
    const QString key = keys.at(QRandomGenerator::global()->bounded(keys.size()));
    auto it = m_purposes.constFind(key);
    return it == m_purposes.constEnd() ? nullptr : &it.value();
}

const ArtifactGeneratorWidget::RarityRule *ArtifactGeneratorWidget::randomRarity() const
{
    if (m_rarities.isEmpty()) {
        return nullptr;
    }
    const QStringList keys = m_rarities.keys();
    const QString key = keys.at(QRandomGenerator::global()->bounded(keys.size()));
    auto it = m_rarities.constFind(key);
    return it == m_rarities.constEnd() ? nullptr : &it.value();
}

void ArtifactGeneratorWidget::generateArtifacts()
{
    if (m_rarities.isEmpty() || m_types.isEmpty() || m_themes.isEmpty() || m_purposes.isEmpty()) {
        resultOutput->setPlainText(QStringLiteral("База артефактов не загружена."));
        return;
    }

    const int count = countSpin->value();
    QStringList rendered;
    rendered.reserve(count);

    for (int i = 0; i < count; ++i) {
        const ArtifactResult artifact = generateOneArtifact();
        rendered << renderArtifact(artifact, i + 1);
    }

    resultOutput->setPlainText(rendered.join(QStringLiteral("\n\n")));
}

void ArtifactGeneratorWidget::generateRandomArtifact()
{
    if (m_rarities.isEmpty() || m_types.isEmpty() || m_themes.isEmpty() || m_purposes.isEmpty()) {
        resultOutput->setPlainText(QStringLiteral("База артефактов не загружена."));
        return;
    }

    const ArtifactResult artifact = generateOneArtifact(true);
    resultOutput->setPlainText(renderArtifact(artifact, 1));
}

