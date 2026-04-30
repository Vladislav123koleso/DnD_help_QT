#include "charactersheet.h"
#include "ui_charactersheet.h"

#include "characterprogressionrules.h"
#include "class.h"
#include "databasemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QString normalizedName(QString value)
{
    return value.simplified().toLower();
}

QString scoreWithModifier(int score)
{
    const int modifier = Character::abilityModifier(score);
    return QString("%1 (%2%3)")
        .arg(score)
        .arg(modifier >= 0 ? "+" : "")
        .arg(modifier);
}

int scoreFromFieldText(const QString &text, int fallback)
{
    const QRegularExpressionMatch match = QRegularExpression(QStringLiteral("(-?\\d+)"))
                                              .match(text.trimmed());
    if (!match.hasMatch()) {
        return fallback;
    }

    bool ok = false;
    const int value = match.captured(1).toInt(&ok);
    return ok ? qMax(1, value) : fallback;
}

QString signedValue(int value)
{
    return QString("%1%2")
        .arg(value >= 0 ? "+" : "")
        .arg(value);
}

QString spellLevelLabel(int level)
{
    return level == 0 ? QStringLiteral("Заговор") : QStringLiteral("%1 ур.").arg(level);
}

QString joinOrDash(const QStringList &values)
{
    return values.isEmpty() ? QStringLiteral("—") : values.join(QStringLiteral(", "));
}

using NamedAbilityEntry = QPair<QString, QString>;

const QList<NamedAbilityEntry> &savingThrowDefinitions()
{
    static const QList<NamedAbilityEntry> definitions = {
        {QStringLiteral("Сила"), QStringLiteral("Сила")},
        {QStringLiteral("Ловкость"), QStringLiteral("Ловкость")},
        {QStringLiteral("Телосложение"), QStringLiteral("Телосложение")},
        {QStringLiteral("Интеллект"), QStringLiteral("Интеллект")},
        {QStringLiteral("Мудрость"), QStringLiteral("Мудрость")},
        {QStringLiteral("Харизма"), QStringLiteral("Харизма")}
    };
    return definitions;
}

const QList<NamedAbilityEntry> &skillDefinitions()
{
    static const QList<NamedAbilityEntry> definitions = {
        {QStringLiteral("Акробатика"), QStringLiteral("Ловкость")},
        {QStringLiteral("Атлетика"), QStringLiteral("Сила")},
        {QStringLiteral("Восприятие"), QStringLiteral("Мудрость")},
        {QStringLiteral("Выживание"), QStringLiteral("Мудрость")},
        {QStringLiteral("Выступление"), QStringLiteral("Харизма")},
        {QStringLiteral("Запугивание"), QStringLiteral("Харизма")},
        {QStringLiteral("История"), QStringLiteral("Интеллект")},
        {QStringLiteral("Ловкость рук"), QStringLiteral("Ловкость")},
        {QStringLiteral("Магия"), QStringLiteral("Интеллект")},
        {QStringLiteral("Медицина"), QStringLiteral("Мудрость")},
        {QStringLiteral("Обман"), QStringLiteral("Харизма")},
        {QStringLiteral("Природа"), QStringLiteral("Интеллект")},
        {QStringLiteral("Проницательность"), QStringLiteral("Мудрость")},
        {QStringLiteral("Расследование"), QStringLiteral("Интеллект")},
        {QStringLiteral("Религия"), QStringLiteral("Интеллект")},
        {QStringLiteral("Скрытность"), QStringLiteral("Ловкость")},
        {QStringLiteral("Убеждение"), QStringLiteral("Харизма")},
        {QStringLiteral("Уход за животными"), QStringLiteral("Мудрость")}
    };
    return definitions;
}

QString proficiencyEntryKey(const QString &section, const QString &name)
{
    return QStringLiteral("%1|%2").arg(section, normalizedName(name));
}

QString abilityShortLabel(const QString &ability)
{
    if (ability == QStringLiteral("Сила")) {
        return QStringLiteral("СИЛ");
    }
    if (ability == QStringLiteral("Ловкость")) {
        return QStringLiteral("ЛОВ");
    }
    if (ability == QStringLiteral("Телосложение")) {
        return QStringLiteral("ТЕЛ");
    }
    if (ability == QStringLiteral("Интеллект")) {
        return QStringLiteral("ИНТ");
    }
    if (ability == QStringLiteral("Мудрость")) {
        return QStringLiteral("МУД");
    }
    if (ability == QStringLiteral("Харизма")) {
        return QStringLiteral("ХАР");
    }
    return ability.left(3).toUpper();
}

int characterAbilityScore(const Character *character, const QString &ability)
{
    if (!character) {
        return 10;
    }
    if (ability == QStringLiteral("Сила")) {
        return character->strength;
    }
    if (ability == QStringLiteral("Ловкость")) {
        return character->dexterity;
    }
    if (ability == QStringLiteral("Телосложение")) {
        return character->constitution;
    }
    if (ability == QStringLiteral("Интеллект")) {
        return character->intelligence;
    }
    if (ability == QStringLiteral("Мудрость")) {
        return character->wisdom;
    }
    if (ability == QStringLiteral("Харизма")) {
        return character->charisma;
    }
    return 10;
}

QStringList normalizedValues(const QStringList &values)
{
    QStringList result;
    result.reserve(values.size());
    for (const QString &value : values) {
        result << normalizedName(value);
    }
    return result;
}

bool isDisplayableRaceTraitTitle(const QString &title)
{
    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.startsWith(QStringLiteral("!"))) {
        return false;
    }

    QString cleanedTitle = trimmedTitle;
    while (cleanedTitle.startsWith(QStringLiteral("!"))) {
        cleanedTitle.remove(0, 1);
        cleanedTitle = cleanedTitle.trimmed();
    }
    if (cleanedTitle.startsWith(QStringLiteral("(заклинание)"), Qt::CaseInsensitive)) {
        cleanedTitle.remove(0, QStringLiteral("(заклинание)").size());
        cleanedTitle = cleanedTitle.trimmed();
    }

    const QString normalizedTitle = normalizedName(cleanedTitle);
    if (normalizedTitle.isEmpty()) {
        return false;
    }

    static const QStringList blockedExactTitles = {
        QStringLiteral("возраст"),
        QStringLiteral("мировоззрение"),
        QStringLiteral("размер"),
        QStringLiteral("рост"),
        QStringLiteral("вес"),
        QStringLiteral("детские имена"),
        QStringLiteral("мужские взрослые имена"),
        QStringLiteral("женские взрослые имена"),
        QStringLiteral("мужские имена"),
        QStringLiteral("женские имена"),
        QStringLiteral("фамилии"),
        QStringLiteral("клановые имена"),
        QStringLiteral("весна"),
        QStringLiteral("лето"),
        QStringLiteral("осень"),
        QStringLiteral("зима")
    };
    if (blockedExactTitles.contains(normalizedTitle)) {
        return false;
    }

    static const QStringList blockedTitleParts = {
        QStringLiteral("имена"),
        QStringLiteral("имя"),
        QStringLiteral("возраст"),
        QStringLiteral("сезон"),
        QStringLiteral("дополнительный язык")
    };
    for (const QString &blockedPart : blockedTitleParts) {
        if (normalizedTitle.contains(blockedPart)) {
            return false;
        }
    }

    return true;
}

QMap<QString, QString> normalizedRaceTraits(const QMap<QString, QString> &traits)
{
    QMap<QString, QString> normalized;
    for (auto it = traits.begin(); it != traits.end(); ++it) {
        QString title = it.key().trimmed();
        while (title.startsWith(QStringLiteral("!"))) {
            title.remove(0, 1);
            title = title.trimmed();
        }
        if (title.startsWith(QStringLiteral("(заклинание)"), Qt::CaseInsensitive)) {
            title.remove(0, QStringLiteral("(заклинание)").size());
            title = title.trimmed();
        }

        QString description = it.value().trimmed();
        description.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]")), QStringLiteral("\\1"));
        if (title.isEmpty() || description.isEmpty()) {
            continue;
        }

        normalized.insert(title.simplified(), description);
    }
    return normalized;
}

QMap<QString, QString> filteredRaceTraits(const QMap<QString, QString> &traits)
{
    QMap<QString, QString> filtered;
    for (auto it = traits.begin(); it != traits.end(); ++it) {
        if (it.key().trimmed().startsWith(QStringLiteral("!"))) {
            continue;
        }

        QString title = it.key().trimmed();
        if (title.startsWith(QStringLiteral("(заклинание)"), Qt::CaseInsensitive)) {
            title.remove(0, QStringLiteral("(заклинание)").size());
            title = title.trimmed();
        }

        QString description = it.value().trimmed();
        description.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]")), QStringLiteral("\\1"));
        if (!isDisplayableRaceTraitTitle(title) || description.isEmpty()) {
            continue;
        }
        filtered.insert(title.simplified(), description);
    }
    return filtered;
}

int minimumRequiredLevelFromText(const QString &text)
{
    int minimumLevel = 1;
    bool matched = false;

    const QRegularExpression regex(
        QStringLiteral("(?:начиная\s+с|с)\s*(\\d+)\\s*(?:-|–)?\\s*(?:го|й|ого)?\\s+уров"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator iterator = regex.globalMatch(text);
    while (iterator.hasNext()) {
        const int level = iterator.next().captured(1).toInt();
        if (level > 1) {
            minimumLevel = matched ? qMin(minimumLevel, level) : level;
            matched = true;
        }
    }

    return minimumLevel;
}

bool raceTraitAvailableAtLevel(const QString &title, const QString &description, int level)
{
    const int requiredLevel = qMax(
        minimumRequiredLevelFromText(title),
        minimumRequiredLevelFromText(description));
    return level >= requiredLevel;
}

bool isPureRaceProficiencyTrait(const QString &title, const QString &description)
{
    const QString normalizedTitle = normalizedName(title);
    const QString normalizedDescription = normalizedName(description);
    return normalizedTitle.contains(QStringLiteral("владение")) ||
           normalizedDescription.startsWith(QStringLiteral("вы владеете ")) ||
           normalizedDescription.startsWith(QStringLiteral("вы получаете владение ")) ||
           normalizedDescription.contains(QStringLiteral("владение навыком")) ||
           normalizedDescription.contains(QStringLiteral("владение навыками"));
}

QMap<QString, QString> displayableRaceFeatures(const QMap<QString, QString> &traits, int characterLevel)
{
    QMap<QString, QString> filtered;
    const QMap<QString, QString> baseTraits = filteredRaceTraits(traits);
    for (auto it = baseTraits.begin(); it != baseTraits.end(); ++it) {
        if (!raceTraitAvailableAtLevel(it.key(), it.value(), characterLevel)) {
            continue;
        }
        if (isPureRaceProficiencyTrait(it.key(), it.value())) {
            continue;
        }
        filtered.insert(it.key(), it.value());
    }
    return filtered;
}

bool spellBelongsToClass(const Spell &spell, const QString &className)
{
    const QString classNeedle = normalizedName(className);
    if (!spell.selectionClass.trimmed().isEmpty()) {
        return normalizedName(spell.selectionClass) == classNeedle;
    }

    const QStringList classParts = spell.classes.split(',', Qt::SkipEmptyParts);
    for (const QString &part : classParts) {
        if (normalizedName(part) == classNeedle) {
            return true;
        }
    }

    return false;
}

bool spellMatchesExact(const Spell &spell, const QString &className, const QString &spellName, int spellLevel)
{
    return spell.level == spellLevel &&
           normalizedName(spell.name) == normalizedName(spellName) &&
           spellBelongsToClass(spell, className);
}

bool spellSortLess(const Spell &left, const Spell &right)
{
    if (left.level != right.level) {
        return left.level < right.level;
    }
    return left.name.localeAwareCompare(right.name) < 0;
}

QString spellUniqueKey(const Spell &spell)
{
    const QString owner = spell.selectionClass.trimmed().isEmpty() ? spell.classes : spell.selectionClass;
    return QString("%1|%2|%3")
        .arg(normalizedName(owner))
        .arg(spell.level)
        .arg(normalizedName(spell.name));
}

QList<Spell> uniqueSortedSpells(QList<Spell> spells)
{
    std::sort(spells.begin(), spells.end(), spellSortLess);

    QList<Spell> unique;
    QStringList seen;
    for (const Spell &spell : spells) {
        const QString key = spellUniqueKey(spell);
        if (!seen.contains(key)) {
            seen << key;
            unique << spell;
        }
    }
    return unique;
}

void clearLayout(QLayout *layout)
{
    if (!layout) {
        return;
    }

    QLayoutItem *item = nullptr;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->layout()) {
            clearLayout(item->layout());
        }
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

QMap<int, int> slotMapFromRow(const QVector<int> &row)
{
    QMap<int, int> result;
    for (int index = 0; index < row.size(); ++index) {
        if (row.at(index) > 0) {
            result.insert(index + 1, row.at(index));
        }
    }
    return result;
}

const QVector<QVector<int>> &fullCasterSlotTable()
{
    static const QVector<QVector<int>> table = {
        {2, 0, 0, 0, 0, 0, 0, 0, 0},
        {3, 0, 0, 0, 0, 0, 0, 0, 0},
        {4, 2, 0, 0, 0, 0, 0, 0, 0},
        {4, 3, 0, 0, 0, 0, 0, 0, 0},
        {4, 3, 2, 0, 0, 0, 0, 0, 0},
        {4, 3, 3, 0, 0, 0, 0, 0, 0},
        {4, 3, 3, 1, 0, 0, 0, 0, 0},
        {4, 3, 3, 2, 0, 0, 0, 0, 0},
        {4, 3, 3, 3, 1, 0, 0, 0, 0},
        {4, 3, 3, 3, 2, 0, 0, 0, 0},
        {4, 3, 3, 3, 2, 1, 0, 0, 0},
        {4, 3, 3, 3, 2, 1, 0, 0, 0},
        {4, 3, 3, 3, 2, 1, 1, 0, 0},
        {4, 3, 3, 3, 2, 1, 1, 0, 0},
        {4, 3, 3, 3, 2, 1, 1, 1, 0},
        {4, 3, 3, 3, 2, 1, 1, 1, 0},
        {4, 3, 3, 3, 2, 1, 1, 1, 1},
        {4, 3, 3, 3, 3, 1, 1, 1, 1},
        {4, 3, 3, 3, 3, 2, 1, 1, 1},
        {4, 3, 3, 3, 3, 2, 2, 1, 1}
    };
    return table;
}

const QVector<QVector<int>> &halfCasterSlotTable()
{
    static const QVector<QVector<int>> table = {
        {0, 0, 0, 0, 0},
        {2, 0, 0, 0, 0},
        {3, 0, 0, 0, 0},
        {3, 0, 0, 0, 0},
        {4, 2, 0, 0, 0},
        {4, 2, 0, 0, 0},
        {4, 3, 0, 0, 0},
        {4, 3, 0, 0, 0},
        {4, 3, 2, 0, 0},
        {4, 3, 2, 0, 0},
        {4, 3, 3, 0, 0},
        {4, 3, 3, 0, 0},
        {4, 3, 3, 1, 0},
        {4, 3, 3, 1, 0},
        {4, 3, 3, 2, 0},
        {4, 3, 3, 2, 0},
        {4, 3, 3, 3, 1},
        {4, 3, 3, 3, 1},
        {4, 3, 3, 3, 2},
        {4, 3, 3, 3, 2}
    };
    return table;
}

const QVector<QVector<int>> &artificerSlotTable()
{
    static const QVector<QVector<int>> table = {
        {2, 0, 0, 0, 0},
        {2, 0, 0, 0, 0},
        {3, 0, 0, 0, 0},
        {3, 0, 0, 0, 0},
        {4, 2, 0, 0, 0},
        {4, 2, 0, 0, 0},
        {4, 3, 0, 0, 0},
        {4, 3, 0, 0, 0},
        {4, 3, 2, 0, 0},
        {4, 3, 2, 0, 0},
        {4, 3, 3, 0, 0},
        {4, 3, 3, 0, 0},
        {4, 3, 3, 1, 0},
        {4, 3, 3, 1, 0},
        {4, 3, 3, 2, 0},
        {4, 3, 3, 2, 0},
        {4, 3, 3, 3, 1},
        {4, 3, 3, 3, 1},
        {4, 3, 3, 3, 2},
        {4, 3, 3, 3, 2}
    };
    return table;
}

struct PactMagicProgression {
    int slotCount;
    int slotLevel;
};

const QVector<PactMagicProgression> &warlockPactTable()
{
    static const QVector<PactMagicProgression> table = {
        {1, 1}, {2, 1}, {2, 2}, {2, 2}, {2, 3},
        {2, 3}, {2, 4}, {2, 4}, {2, 5}, {2, 5},
        {3, 5}, {3, 5}, {3, 5}, {3, 5}, {3, 5},
        {3, 5}, {4, 5}, {4, 5}, {4, 5}, {4, 5}
    };
    return table;
}

QMap<int, int> spellSlotsForClassLevel(const QString &className, int classLevel)
{
    if (classLevel <= 0) {
        return {};
    }

    const QString normalizedClass = normalizedName(className);
    const int rowIndex = qBound(0, classLevel - 1, 19);

    if (normalizedClass == normalizedName(QStringLiteral("Бард")) ||
        normalizedClass == normalizedName(QStringLiteral("Жрец")) ||
        normalizedClass == normalizedName(QStringLiteral("Друид")) ||
        normalizedClass == normalizedName(QStringLiteral("Волшебник")) ||
        normalizedClass == normalizedName(QStringLiteral("Чародей"))) {
        return slotMapFromRow(fullCasterSlotTable().at(rowIndex));
    }

    if (normalizedClass == normalizedName(QStringLiteral("Паладин")) ||
        normalizedClass == normalizedName(QStringLiteral("Следопыт"))) {
        return slotMapFromRow(halfCasterSlotTable().at(rowIndex));
    }

    if (normalizedClass == normalizedName(QStringLiteral("Изобретатель"))) {
        return slotMapFromRow(artificerSlotTable().at(rowIndex));
    }

    if (normalizedClass == normalizedName(QStringLiteral("Колдун"))) {
        const PactMagicProgression progression = warlockPactTable().at(rowIndex);
        if (progression.slotCount > 0 && progression.slotLevel > 0) {
            return {{progression.slotLevel, progression.slotCount}};
        }
    }

    return {};
}

QString resolveClassesJsonPath()
{
    QStringList candidates;
    candidates << QStringLiteral("classes_dndsu.json");
    candidates << QDir::current().filePath(QStringLiteral("classes_dndsu.json"));

    QDir appDir(QCoreApplication::applicationDirPath());
    candidates << appDir.filePath(QStringLiteral("classes_dndsu.json"));

    QDir dir = appDir;
    for (int index = 0; index < 6; ++index) {
        candidates << dir.filePath(QStringLiteral("classes_dndsu.json"));
        if (!dir.cdUp()) {
            break;
        }
    }

    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return QString();
}

const QMap<QString, Class> &classDefinitions()
{
    static bool loaded = false;
    static QMap<QString, Class> definitions;

    if (loaded) {
        return definitions;
    }

    loaded = true;
    QFile file(resolveClassesJsonPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return definitions;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
        return definitions;
    }

    const QJsonArray classes = document.array();
    for (const QJsonValue &value : classes) {
        const Class cls = Class::fromJson(value.toObject());

        if (!cls.name.isEmpty()) {
            definitions.insert(cls.name, cls);
        }
    }

    return definitions;
}

}

CharacterSheet::CharacterSheet(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::CharacterSheet)
{
    ui->setupUi(this);
    ui->nameEdit->setReadOnly(false);
    ui->nameEdit->setClearButtonEnabled(true);

    const QStringList alignments = {
        QString(),
        QStringLiteral("Законно добрый"), QStringLiteral("Нейтрально добрый"), QStringLiteral("Хаотический добрый"),
        QStringLiteral("Законно нейтральный"), QStringLiteral("Истинно нейтральный"), QStringLiteral("Хаотический нейтральный"),
        QStringLiteral("Законно злой"), QStringLiteral("Нейтрально злой"), QStringLiteral("Хаотический злой")
    };
    ui->alignmentCombo->addItems(alignments);

    setupExtendedSections();
    m_allSpells = DatabaseManager::instance().getAllSpells();

    connect(ui->nameEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->backgroundEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->alignmentCombo, &QComboBox::textActivated, this, &CharacterSheet::onFieldChanged);
    connect(ui->ageEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->heightEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->weightEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->skinEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->hairEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->hpCurrentEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->hpTempEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->appearanceEdit, &QTextEdit::textChanged, this, &CharacterSheet::onFieldChanged);
    connect(historyEdit, &QTextEdit::textChanged, this, &CharacterSheet::onFieldChanged);
    for (QLineEdit *abilityEdit : {ui->strEdit, ui->dexEdit, ui->conEdit, ui->intEdit, ui->wisEdit, ui->chaEdit}) {
        connect(abilityEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    }
}

CharacterSheet::~CharacterSheet()
{
    delete ui;
}

void CharacterSheet::setupExtendedSections()
{
    ui->horizontalLayout_Main->setSpacing(8);
    ui->gridLayout_Combat->setHorizontalSpacing(8);
    ui->gridLayout_Combat->setVerticalSpacing(4);
    ui->statsGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->combatGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    ui->appearanceGroup->hide();
    ui->verticalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Fixed);
    ui->verticalLayout_Stats->invalidate();

    for (QLineEdit *abilityEdit : {ui->strEdit, ui->dexEdit, ui->conEdit, ui->intEdit, ui->wisEdit, ui->chaEdit}) {
        QFont font = abilityEdit->font();
        font.setPointSize(12);
        abilityEdit->setFont(font);
        abilityEdit->setMaximumWidth(92);
        abilityEdit->setMinimumHeight(28);
    }

    detailsTabs = new QTabWidget(this);
    detailsTabs->setDocumentMode(true);
    ui->verticalLayout->addWidget(detailsTabs, 1);

    QWidget *historyTab = new QWidget(detailsTabs);
    QVBoxLayout *historyLayout = new QVBoxLayout(historyTab);
    QGroupBox *historyGroup = new QGroupBox(QStringLiteral("История персонажа"), historyTab);
    QVBoxLayout *historyGroupLayout = new QVBoxLayout(historyGroup);
    historyEdit = new QTextEdit(historyGroup);
    historyEdit->setPlaceholderText(QStringLiteral("Происхождение, мотивация, связи, важные события и личная история персонажа."));
    historyGroup->setTitle(QStringLiteral("История и внешность"));

    QGridLayout *appearanceLayout = new QGridLayout();
    appearanceLayout->setHorizontalSpacing(8);
    appearanceLayout->setVerticalSpacing(6);
    appearanceLayout->setColumnStretch(1, 1);
    appearanceLayout->setColumnStretch(3, 1);
    appearanceLayout->addWidget(ui->label_age, 0, 0);
    appearanceLayout->addWidget(ui->ageEdit, 0, 1);
    appearanceLayout->addWidget(ui->label_height, 0, 2);
    appearanceLayout->addWidget(ui->heightEdit, 0, 3);
    appearanceLayout->addWidget(ui->label_weight, 1, 0);
    appearanceLayout->addWidget(ui->weightEdit, 1, 1);
    appearanceLayout->addWidget(ui->label_skin, 1, 2);
    appearanceLayout->addWidget(ui->skinEdit, 1, 3);
    appearanceLayout->addWidget(ui->label_hair, 2, 0);
    appearanceLayout->addWidget(ui->hairEdit, 2, 1, 1, 3);
    historyGroupLayout->addLayout(appearanceLayout);
    historyGroupLayout->addWidget(ui->label_desc);
    ui->appearanceEdit->setMinimumHeight(90);
    historyGroupLayout->addWidget(ui->appearanceEdit);

    QFrame *historyDivider = new QFrame(historyGroup);
    historyDivider->setFrameShape(QFrame::HLine);
    historyDivider->setFrameShadow(QFrame::Sunken);
    historyGroupLayout->addWidget(historyDivider);

    QLabel *historyTitleLabel = new QLabel(QStringLiteral("Личная история"), historyGroup);
    QFont historyTitleFont = historyTitleLabel->font();
    historyTitleFont.setBold(true);
    historyGroupLayout->addWidget(historyTitleLabel);
    historyTitleLabel->setFont(historyTitleFont);
    historyGroupLayout->addWidget(historyEdit, 1);
    historyLayout->addWidget(historyGroup);
    historyLayout->addStretch();
    detailsTabs->addTab(historyTab, QStringLiteral("История"));

    QGroupBox *overviewGroup = new QGroupBox(QStringLiteral("Навыки и спасброски"), this);
    overviewGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    QVBoxLayout *overviewLayout = new QVBoxLayout(overviewGroup);
    overviewLayout->setSpacing(8);

    QHBoxLayout *bonusLayout = new QHBoxLayout();
    QLabel *bonusCaptionLabel = new QLabel(QStringLiteral("Бонус мастерства"), overviewGroup);
    proficiencyBonusValueLabel = new QLabel(QStringLiteral("—"), overviewGroup);
    QFont bonusFont = proficiencyBonusValueLabel->font();
    bonusFont.setBold(true);
    bonusFont.setPointSize(14);
    proficiencyBonusValueLabel->setFont(bonusFont);
    proficiencyBonusValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bonusLayout->addWidget(bonusCaptionLabel);
    bonusLayout->addStretch();
    bonusLayout->addWidget(proficiencyBonusValueLabel);
    overviewLayout->addLayout(bonusLayout);

    QHBoxLayout *overviewColumnsLayout = new QHBoxLayout();
    overviewColumnsLayout->setSpacing(16);

    auto addOverviewColumn = [&](const QString &title, const QList<NamedAbilityEntry> &entries, const QString &section) {
        QWidget *columnWidget = new QWidget(overviewGroup);
        QVBoxLayout *columnLayout = new QVBoxLayout(columnWidget);
        columnLayout->setContentsMargins(0, 0, 0, 0);
        columnLayout->setSpacing(4);

        QLabel *titleLabel = new QLabel(title, columnWidget);
        QFont titleFont = titleLabel->font();
        titleFont.setBold(true);
        titleLabel->setFont(titleFont);
        columnLayout->addWidget(titleLabel);

        QGridLayout *grid = new QGridLayout();
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(2);

        int row = 0;
        for (const NamedAbilityEntry &entry : entries) {
            QLabel *indicatorLabel = new QLabel(QStringLiteral("○"), columnWidget);
            indicatorLabel->setAlignment(Qt::AlignCenter);
            indicatorLabel->setMinimumWidth(18);
            QFont indicatorFont = indicatorLabel->font();
            indicatorFont.setPointSize(14);
            indicatorLabel->setFont(indicatorFont);

            QLabel *nameLabel = new QLabel(entry.first, columnWidget);
            QLabel *abilityLabel = new QLabel(abilityShortLabel(entry.second), columnWidget);
            QLabel *valueLabel = new QLabel(QStringLiteral("—"), columnWidget);
            abilityLabel->setStyleSheet(QStringLiteral("color: #6b7280;"));
            valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            valueLabel->setMinimumWidth(36);
            valueLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));

            grid->addWidget(indicatorLabel, row, 0);
            grid->addWidget(nameLabel, row, 1);
            grid->addWidget(abilityLabel, row, 2);
            grid->addWidget(valueLabel, row, 3);

            const QString key = proficiencyEntryKey(section, entry.first);
            proficiencyIndicatorLabels.insert(key, indicatorLabel);
            proficiencyValueLabels.insert(key, valueLabel);
            ++row;
        }

        columnLayout->addLayout(grid);
        columnLayout->addStretch();
        overviewColumnsLayout->addWidget(columnWidget, 1);
    };

    addOverviewColumn(QStringLiteral("Спасброски"), savingThrowDefinitions(), QStringLiteral("save"));

    QList<NamedAbilityEntry> skillColumnOne;
    QList<NamedAbilityEntry> skillColumnTwo;
    const QList<NamedAbilityEntry> allSkills = skillDefinitions();
    const int skillSplitIndex = (allSkills.size() + 1) / 2;
    for (int index = 0; index < allSkills.size(); ++index) {
        if (index < skillSplitIndex) {
            skillColumnOne.append(allSkills.at(index));
        } else {
            skillColumnTwo.append(allSkills.at(index));
        }
    }
    addOverviewColumn(QStringLiteral("Навыки"), skillColumnOne, QStringLiteral("skill"));
    addOverviewColumn(QStringLiteral("Навыки"), skillColumnTwo, QStringLiteral("skill"));

    overviewLayout->addLayout(overviewColumnsLayout);

    const int detailsTabsIndex = ui->verticalLayout->indexOf(detailsTabs);
    if (detailsTabsIndex >= 0) {
        ui->verticalLayout->insertWidget(detailsTabsIndex, overviewGroup);
    } else {
        ui->verticalLayout->addWidget(overviewGroup);
    }

    QWidget *spellTab = new QWidget(detailsTabs);
    QVBoxLayout *spellLayout = new QVBoxLayout(spellTab);

    spellcastingSummaryLabel = new QLabel(spellTab);
    spellcastingSummaryLabel->setWordWrap(true);
    spellLayout->addWidget(spellcastingSummaryLabel);

    QGroupBox *slotsGroup = new QGroupBox(QStringLiteral("Ячейки заклинаний"), spellTab);
    QVBoxLayout *slotsGroupLayout = new QVBoxLayout(slotsGroup);
    QScrollArea *slotsScrollArea = new QScrollArea(slotsGroup);
    slotsScrollArea->setWidgetResizable(true);
    slotsScrollArea->setFrameShape(QFrame::NoFrame);
    spellSlotsContainer = new QWidget(slotsScrollArea);
    spellSlotsLayout = new QVBoxLayout(spellSlotsContainer);
    spellSlotsLayout->setContentsMargins(0, 0, 0, 0);
    slotsScrollArea->setWidget(spellSlotsContainer);
    slotsGroupLayout->addWidget(slotsScrollArea);
    spellLayout->addWidget(slotsGroup);

    QGroupBox *spellListGroup = new QGroupBox(QStringLiteral("Заклинания персонажа"), spellTab);
    QVBoxLayout *spellListLayout = new QVBoxLayout(spellListGroup);
    QHBoxLayout *spellFilterLayout = new QHBoxLayout();

    spellClassFilter = new QComboBox(spellListGroup);
    spellClassFilter->addItem(QStringLiteral("Все классы"), QString());
    spellLevelFilter = new QComboBox(spellListGroup);
    spellLevelFilter->addItem(QStringLiteral("Все уровни"), -1);
    spellLevelFilter->addItem(QStringLiteral("Заговор"), 0);
    for (int level = 1; level <= 9; ++level) {
        spellLevelFilter->addItem(QStringLiteral("%1 уровень").arg(level), level);
    }
    spellStateFilter = new QComboBox(spellListGroup);
    spellStateFilter->addItem(QStringLiteral("Все состояния"), QStringLiteral("all"));
    spellStateFilter->addItem(QStringLiteral("Подготовленные / известные"), QStringLiteral("active"));
    spellStateFilter->addItem(QStringLiteral("Готовые к касту"), QStringLiteral("castable"));
    spellSearchEdit = new QLineEdit(spellListGroup);
    spellSearchEdit->setPlaceholderText(QStringLiteral("Поиск по названию, школе или описанию..."));

    spellFilterLayout->addWidget(new QLabel(QStringLiteral("Класс:"), spellListGroup));
    spellFilterLayout->addWidget(spellClassFilter);
    spellFilterLayout->addWidget(new QLabel(QStringLiteral("Уровень:"), spellListGroup));
    spellFilterLayout->addWidget(spellLevelFilter);
    spellFilterLayout->addWidget(new QLabel(QStringLiteral("Состояние:"), spellListGroup));
    spellFilterLayout->addWidget(spellStateFilter);
    spellFilterLayout->addWidget(spellSearchEdit, 1);
    spellListLayout->addLayout(spellFilterLayout);

    QSplitter *spellSplitter = new QSplitter(Qt::Horizontal, spellListGroup);
    spellTree = new QTreeWidget(spellSplitter);
    spellTree->setHeaderLabels({QStringLiteral("Статус"), QStringLiteral("Заклинание"), QStringLiteral("Уровень"), QStringLiteral("Класс"), QStringLiteral("Каст")});
    spellTree->setAlternatingRowColors(true);
    spellTree->setRootIsDecorated(true);
    spellTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    spellTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    spellTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    spellTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    spellTree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

    QWidget *spellDetailsWidget = new QWidget(spellSplitter);
    QVBoxLayout *spellDetailsLayout = new QVBoxLayout(spellDetailsWidget);
    spellDetailsTitleLabel = new QLabel(QStringLiteral("Выберите заклинание"), spellDetailsWidget);
    QFont spellTitleFont = spellDetailsTitleLabel->font();
    spellTitleFont.setBold(true);
    spellTitleFont.setPointSize(14);
    spellDetailsTitleLabel->setFont(spellTitleFont);
    spellDetailsMetaLabel = new QLabel(spellDetailsWidget);
    spellDetailsMetaLabel->setWordWrap(true);
    spellDetailsText = new QTextEdit(spellDetailsWidget);
    spellDetailsText->setReadOnly(true);
    spellDetailsLayout->addWidget(spellDetailsTitleLabel);
    spellDetailsLayout->addWidget(spellDetailsMetaLabel);
    spellDetailsLayout->addWidget(spellDetailsText, 1);

    spellSplitter->setStretchFactor(0, 3);
    spellSplitter->setStretchFactor(1, 2);
    spellListLayout->addWidget(spellSplitter, 1);
    spellLayout->addWidget(spellListGroup, 1);
    detailsTabs->addTab(spellTab, QStringLiteral("Заклинания"));

    QWidget *featuresTab = new QWidget(detailsTabs);
    QVBoxLayout *featuresLayout = new QVBoxLayout(featuresTab);

    QGroupBox *proficiencyGroup = new QGroupBox(QStringLiteral("Владения и знания"), featuresTab);
    QFormLayout *proficiencyLayout = new QFormLayout(proficiencyGroup);
    languagesValueLabel = new QLabel(proficiencyGroup);
    skillsValueLabel = new QLabel(proficiencyGroup);
    toolsValueLabel = new QLabel(proficiencyGroup);
    savesValueLabel = new QLabel(proficiencyGroup);
    armorValueLabel = new QLabel(proficiencyGroup);
    weaponsValueLabel = new QLabel(proficiencyGroup);

    for (QLabel *label : {languagesValueLabel, skillsValueLabel, toolsValueLabel, savesValueLabel, armorValueLabel, weaponsValueLabel}) {
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }

    proficiencyLayout->addRow(QStringLiteral("Языки:"), languagesValueLabel);
    proficiencyLayout->addRow(QStringLiteral("Навыки:"), skillsValueLabel);
    proficiencyLayout->addRow(QStringLiteral("Инструменты:"), toolsValueLabel);
    proficiencyLayout->addRow(QStringLiteral("Спасброски:"), savesValueLabel);
    proficiencyLayout->addRow(QStringLiteral("Доспехи:"), armorValueLabel);
    proficiencyLayout->addRow(QStringLiteral("Оружие:"), weaponsValueLabel);
    featuresLayout->addWidget(proficiencyGroup);

    QSplitter *featuresSplitter = new QSplitter(Qt::Horizontal, featuresTab);
    featuresTree = new QTreeWidget(featuresSplitter);
    featuresTree->setHeaderLabels({QStringLiteral("Источник"), QStringLiteral("Умение / блок")});
    featuresTree->setAlternatingRowColors(true);
    featuresTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    featuresTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);

    QWidget *featuresRightWidget = new QWidget(featuresSplitter);
    QVBoxLayout *featuresRightLayout = new QVBoxLayout(featuresRightWidget);

    featureDetailsText = new QTextEdit(featuresRightWidget);
    featureDetailsText->setReadOnly(true);
    featuresRightLayout->addWidget(featureDetailsText, 1);

    QGroupBox *listsGroup = new QGroupBox(QStringLiteral("Инвентарь и атаки"), featuresRightWidget);
    QHBoxLayout *listsLayout = new QHBoxLayout(listsGroup);
    inventoryList = new QListWidget(listsGroup);
    attacksList = new QListWidget(listsGroup);
    inventoryList->setAlternatingRowColors(true);
    attacksList->setAlternatingRowColors(true);
    listsLayout->addWidget(inventoryList, 1);
    listsLayout->addWidget(attacksList, 1);
    featuresRightLayout->addWidget(listsGroup);

    featuresSplitter->setStretchFactor(0, 2);
    featuresSplitter->setStretchFactor(1, 3);
    featuresLayout->addWidget(featuresSplitter, 1);
    detailsTabs->addTab(featuresTab, QStringLiteral("Умения и владения"));

    connect(spellClassFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CharacterSheet::onSpellFiltersChanged);
    connect(spellLevelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CharacterSheet::onSpellFiltersChanged);
    connect(spellStateFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CharacterSheet::onSpellFiltersChanged);
    connect(spellSearchEdit, &QLineEdit::textChanged, this, &CharacterSheet::onSpellFiltersChanged);
    connect(spellTree, &QTreeWidget::itemChanged, this, &CharacterSheet::onSpellItemChanged);
    connect(spellTree, &QTreeWidget::itemSelectionChanged, this, &CharacterSheet::onSpellSelectionChanged);
    connect(featuresTree, &QTreeWidget::itemSelectionChanged, this, &CharacterSheet::onFeatureSelectionChanged);

    clearExtendedSections();
}

void CharacterSheet::setCharacter(Character *c)
{
    m_character = c;

    if (!m_character) {
        ui->nameEdit->clear();
        ui->classLevelEdit->clear();
        ui->raceEdit->clear();
        ui->xpEdit->clear();
        ui->acEdit->clear();
        ui->initiativeEdit->clear();
        ui->speedEdit->clear();
        ui->hpMaxEdit->clear();
        ui->hpCurrentEdit->clear();
        ui->hpTempEdit->clear();
        ui->backgroundEdit->clear();
        ui->appearanceEdit->clear();
        ui->ageEdit->clear();
        ui->heightEdit->clear();
        ui->weightEdit->clear();
        ui->skinEdit->clear();
        ui->hairEdit->clear();
        ui->strEdit->clear();
        ui->dexEdit->clear();
        ui->conEdit->clear();
        ui->intEdit->clear();
        ui->wisEdit->clear();
        ui->chaEdit->clear();
        ui->alignmentCombo->setCurrentIndex(0);
        ui->raceEdit->setToolTip(QString());
        ui->classLevelEdit->setToolTip(QString());
        ui->backgroundEdit->setToolTip(QString());
        clearExtendedSections();
        return;
    }

    updateFromCharacter();
}

void CharacterSheet::clearExtendedSections()
{
    if (historyEdit) {
        historyEdit->clear();
    }

    if (spellcastingSummaryLabel) {
        spellcastingSummaryLabel->setText(QStringLiteral("У персонажа пока нет магии или заклинания ещё не выбраны."));
    }

    if (spellSlotsLayout) {
        clearLayout(spellSlotsLayout);
    }

    if (spellTree) {
        spellTree->clear();
    }

    if (spellDetailsTitleLabel) {
        spellDetailsTitleLabel->setText(QStringLiteral("Выберите заклинание"));
    }
    if (spellDetailsMetaLabel) {
        spellDetailsMetaLabel->clear();
    }
    if (spellDetailsText) {
        spellDetailsText->setPlainText(QStringLiteral("Здесь будет полное описание выбранного заклинания и его текущий статус."));
    }

    if (featuresTree) {
        featuresTree->clear();
    }
    if (featureDetailsText) {
        featureDetailsText->setPlainText(QStringLiteral("Здесь будут описания расовых, классовых, фоновых и прочих особенностей персонажа."));
    }

    updateOverviewPanel();

    for (QLabel *label : {languagesValueLabel, skillsValueLabel, toolsValueLabel, savesValueLabel, armorValueLabel, weaponsValueLabel}) {
        if (label) {
            label->setText(QStringLiteral("—"));
        }
    }

    if (inventoryList) {
        inventoryList->clear();
    }
    if (attacksList) {
        attacksList->clear();
    }
}

void CharacterSheet::updateFromCharacter()
{
    if (!m_character) {
        return;
    }

    m_updatingUi = true;

    {
        QSignalBlocker blocker(ui->nameEdit);
        ui->nameEdit->setText(m_character->name());
    }

    QString classSummary = m_character->characterClass().trimmed();
    if (classSummary.isEmpty()) {
        ui->classLevelEdit->setText(QStringLiteral("Уровень %1").arg(m_character->level));
    } else {
        ui->classLevelEdit->setText(QStringLiteral("%1 (общий ур. %2)").arg(classSummary).arg(m_character->level));
    }
    ui->raceEdit->setText(m_character->race());

    ui->strEdit->setText(scoreWithModifier(m_character->strength));
    ui->dexEdit->setText(scoreWithModifier(m_character->dexterity));
    ui->conEdit->setText(scoreWithModifier(m_character->constitution));
    ui->intEdit->setText(scoreWithModifier(m_character->intelligence));
    ui->wisEdit->setText(scoreWithModifier(m_character->wisdom));
    ui->chaEdit->setText(scoreWithModifier(m_character->charisma));

    ui->acEdit->setText(QString::number(m_character->armorClass));
    ui->initiativeEdit->setText(signedValue(m_character->initiative));
    ui->speedEdit->setText(
        m_character->flyingSpeed > 0
            ? QStringLiteral("%1 / полет %2").arg(m_character->speed).arg(m_character->flyingSpeed)
            : QString::number(m_character->speed));
    ui->xpEdit->setText(QString::number(m_character->experiencePoints));
    ui->hpMaxEdit->setText(QString::number(m_character->maxHp));

    {
        QSignalBlocker blocker(ui->hpCurrentEdit);
        ui->hpCurrentEdit->setText(QString::number(m_character->currentHp));
    }
    {
        QSignalBlocker blocker(ui->hpTempEdit);
        ui->hpTempEdit->setText(QString::number(m_character->tempHp));
    }
    {
        QSignalBlocker blocker(ui->backgroundEdit);
        ui->backgroundEdit->setText(m_character->background);
    }
    {
        QSignalBlocker blocker(ui->appearanceEdit);
        ui->appearanceEdit->setPlainText(m_character->appearance);
    }
    {
        QSignalBlocker blocker(ui->ageEdit);
        ui->ageEdit->setText(m_character->age);
    }
    {
        QSignalBlocker blocker(ui->heightEdit);
        ui->heightEdit->setText(m_character->height);
    }
    {
        QSignalBlocker blocker(ui->weightEdit);
        ui->weightEdit->setText(m_character->weight);
    }
    {
        QSignalBlocker blocker(ui->skinEdit);
        ui->skinEdit->setText(m_character->skin);
    }
    {
        QSignalBlocker blocker(ui->hairEdit);
        ui->hairEdit->setText(m_character->hair);
    }

    {
        QSignalBlocker blocker(ui->alignmentCombo);
        const int alignmentIndex = ui->alignmentCombo->findText(m_character->alignment);
        ui->alignmentCombo->setCurrentIndex(alignmentIndex >= 0 ? alignmentIndex : 0);
    }

    QStringList raceTooltipLines;
    raceTooltipLines << QStringLiteral("Размер: %1").arg(m_character->size);
    raceTooltipLines << (m_character->flyingSpeed > 0
        ? QStringLiteral("Скорость: %1, полет: %2").arg(m_character->speed).arg(m_character->flyingSpeed)
        : QStringLiteral("Скорость: %1").arg(m_character->speed));
    if (!m_character->languages.isEmpty()) {
        raceTooltipLines << QStringLiteral("Языки: %1").arg(m_character->languages.join(QStringLiteral(", ")));
    }
    const QMap<QString, QString> displayableRaceTraits = displayableRaceFeatures(m_character->traits, m_character->level);
    if (!displayableRaceTraits.isEmpty()) {
        raceTooltipLines << QStringLiteral("Черты: %1").arg(displayableRaceTraits.keys().join(QStringLiteral(", ")));
    }
    ui->raceEdit->setToolTip(raceTooltipLines.join(QStringLiteral("\n")));

    QStringList classTooltipLines;
    if (m_character->hitDie > 0) {
        classTooltipLines << QStringLiteral("Кость хитов: 1d%1").arg(m_character->hitDie);
    }
    classTooltipLines << QStringLiteral("Бонус мастерства: %1").arg(signedValue(m_character->proficiencyBonus));
    if (!m_character->savingThrowProficiencies.isEmpty()) {
        classTooltipLines << QStringLiteral("Спасброски: %1").arg(m_character->savingThrowProficiencies.join(QStringLiteral(", ")));
    }
    if (!m_character->armorProficiencies.isEmpty()) {
        classTooltipLines << QStringLiteral("Доспехи: %1").arg(m_character->armorProficiencies.join(QStringLiteral(", ")));
    }
    if (!m_character->weaponProficiencies.isEmpty()) {
        classTooltipLines << QStringLiteral("Оружие: %1").arg(m_character->weaponProficiencies.join(QStringLiteral(", ")));
    }
    ui->classLevelEdit->setToolTip(classTooltipLines.join(QStringLiteral("\n")));

    QStringList backgroundTooltipLines;
    if (!m_character->backgroundDescription.isEmpty()) {
        backgroundTooltipLines << m_character->backgroundDescription;
    }
    if (!m_character->backgroundFeatureName.isEmpty()) {
        backgroundTooltipLines << QStringLiteral("\nУмение: %1").arg(m_character->backgroundFeatureName);
    }
    if (!m_character->backgroundFeatureDescription.isEmpty()) {
        backgroundTooltipLines << m_character->backgroundFeatureDescription;
    }
    if (!m_character->featNames.isEmpty()) {
        backgroundTooltipLines << QStringLiteral("\nЧерты: %1").arg(m_character->featNames.join(QStringLiteral(", ")));
    }
    ui->backgroundEdit->setToolTip(backgroundTooltipLines.join(QStringLiteral("\n")));

    updateNarrativeSection();
    updateSpellcastingSection();
    updateFeatureSection();
    updateOverviewPanel();

    m_updatingUi = false;
}

void CharacterSheet::updateNarrativeSection()
{
    if (!historyEdit) {
        return;
    }

    QSignalBlocker blocker(historyEdit);
    historyEdit->setPlainText(m_character ? m_character->personalHistory : QString());
}

void CharacterSheet::updateOverviewPanel()
{
    if (proficiencyBonusValueLabel) {
        proficiencyBonusValueLabel->setText(m_character ? signedValue(m_character->proficiencyBonus) : QStringLiteral("—"));
    }

    for (auto it = proficiencyIndicatorLabels.begin(); it != proficiencyIndicatorLabels.end(); ++it) {
        if (!it.value()) {
            continue;
        }
        it.value()->setText(QStringLiteral("○"));
        it.value()->setStyleSheet(QStringLiteral("color: #9ca3af;"));
    }
    for (auto it = proficiencyValueLabels.begin(); it != proficiencyValueLabels.end(); ++it) {
        if (it.value()) {
            it.value()->setText(QStringLiteral("—"));
        }
    }

    if (!m_character) {
        return;
    }

    const QStringList normalizedSkillProficiencies = normalizedValues(m_character->skillProficiencies);
    const QStringList normalizedSavingThrowProficiencies = normalizedValues(m_character->savingThrowProficiencies);

    auto applyEntry = [&](const QString &section, const QString &name, const QString &ability, bool proficient) {
        const QString key = proficiencyEntryKey(section, name);
        if (QLabel *indicatorLabel = proficiencyIndicatorLabels.value(key, nullptr)) {
            indicatorLabel->setText(proficient ? QStringLiteral("●") : QStringLiteral("○"));
            indicatorLabel->setStyleSheet(proficient
                ? QStringLiteral("color: #111827;")
                : QStringLiteral("color: #9ca3af;"));
        }
        if (QLabel *valueLabel = proficiencyValueLabels.value(key, nullptr)) {
            const int baseModifier = Character::abilityModifier(characterAbilityScore(m_character, ability));
            const int totalModifier = baseModifier + (proficient ? m_character->proficiencyBonus : 0);
            valueLabel->setText(signedValue(totalModifier));
        }
    };

    for (const NamedAbilityEntry &entry : savingThrowDefinitions()) {
        applyEntry(
            QStringLiteral("save"),
            entry.first,
            entry.second,
            normalizedSavingThrowProficiencies.contains(normalizedName(entry.first)));
    }
    for (const NamedAbilityEntry &entry : skillDefinitions()) {
        applyEntry(
            QStringLiteral("skill"),
            entry.first,
            entry.second,
            normalizedSkillProficiencies.contains(normalizedName(entry.first)));
    }
}

QStringList CharacterSheet::spellcastingClasses() const
{
    if (!m_character) {
        return {};
    }

    QStringList classes = m_character->classOrder;
    if (classes.isEmpty()) {
        classes = m_character->classLevels.keys();
    }

    QStringList result;
    for (const QString &className : classes) {
        const int classLevel = m_character->classLevels.value(className, 0);
        const SpellSelectionRule rule = CharacterProgressionRules::instance().spellSelectionRuleForClass(m_character, className, classLevel);
        if (classLevel > 0 && rule.isValid()) {
            result << className;
        }
    }
    return result;
}

QMap<int, int> CharacterSheet::spellSlotMaximumsForClass(const QString &className) const
{
    if (!m_character) {
        return {};
    }
    return spellSlotsForClassLevel(className, m_character->classLevels.value(className, 0));
}

QString CharacterSheet::spellSlotKey(const QString &className, int slotLevel) const
{
    return QStringLiteral("%1|%2").arg(className).arg(slotLevel);
}

int CharacterSheet::spellSlotCurrentValue(const QString &className, int slotLevel, int maximum) const
{
    if (!m_character) {
        return 0;
    }

    const int value = m_character->spellSlotCurrent.value(spellSlotKey(className, slotLevel), maximum);
    return qBound(0, value, maximum);
}

bool CharacterSheet::canCastSpellForClass(const QString &className, int spellLevel) const
{
    if (spellLevel == 0) {
        return true;
    }

    const QMap<int, int> maximums = spellSlotMaximumsForClass(className);
    for (auto it = maximums.begin(); it != maximums.end(); ++it) {
        if (it.key() >= spellLevel && spellSlotCurrentValue(className, it.key(), it.value()) > 0) {
            return true;
        }
    }

    return false;
}

QList<Spell> CharacterSheet::knownSpellsForClass(const QString &className) const
{
    if (!m_character) {
        return {};
    }

    QList<Spell> result;
    for (const Spell &spell : m_character->spells) {
        if (spellBelongsToClass(spell, className)) {
            result << spell;
        }
    }
    return uniqueSortedSpells(result);
}

QList<Spell> CharacterSheet::spellbookForClass(const QString &className) const
{
    if (!m_character) {
        return {};
    }

    QList<Spell> result;
    for (const Spell &spell : m_character->spellbook) {
        if (spellBelongsToClass(spell, className)) {
            result << spell;
        }
    }
    return uniqueSortedSpells(result);
}

QList<Spell> CharacterSheet::availablePreparedSpellsForClass(const QString &className, int classLevel, int maxSpellLevel) const
{
    if (classLevel <= 0 || maxSpellLevel <= 0) {
        return {};
    }

    QList<Spell> result;
    for (const Spell &spell : m_allSpells) {
        if (spell.level <= 0 || spell.level > maxSpellLevel) {
            continue;
        }
        if (!spellBelongsToClass(spell, className)) {
            continue;
        }

        Spell entry = spell;
        entry.selectionClass = className;
        result << entry;
    }

    return uniqueSortedSpells(result);
}

Spell CharacterSheet::findSpellForClass(const QString &className, const QString &spellName, int spellLevel) const
{
    const QList<Spell> currentKnown = knownSpellsForClass(className);
    for (const Spell &spell : currentKnown) {
        if (spellMatchesExact(spell, className, spellName, spellLevel)) {
            return spell;
        }
    }

    const QList<Spell> currentSpellbook = spellbookForClass(className);
    for (const Spell &spell : currentSpellbook) {
        if (spellMatchesExact(spell, className, spellName, spellLevel)) {
            return spell;
        }
    }

    if (m_character) {
        const int classLevel = m_character->classLevels.value(className, 0);
        const SpellSelectionRule rule = CharacterProgressionRules::instance().spellSelectionRuleForClass(m_character, className, classLevel);
        const QList<Spell> available = availablePreparedSpellsForClass(className, classLevel, rule.maxSpellLevel);
        for (const Spell &spell : available) {
            if (spellMatchesExact(spell, className, spellName, spellLevel)) {
                return spell;
            }
        }
    }

    for (const Spell &spell : m_allSpells) {
        if (spell.level == spellLevel &&
            normalizedName(spell.name) == normalizedName(spellName) &&
            spellBelongsToClass(spell, className)) {
            Spell entry = spell;
            entry.selectionClass = className;
            return entry;
        }
    }

    return Spell();
}

int CharacterSheet::preparedSpellLimitForClass(const QString &className) const
{
    if (!m_character) {
        return 0;
    }

    const int classLevel = m_character->classLevels.value(className, 0);
    const SpellSelectionRule rule = CharacterProgressionRules::instance().spellSelectionRuleForClass(m_character, className, classLevel);
    if (rule.usesSpellbook) {
        return rule.preparedLimit;
    }
    if (rule.mode == QStringLiteral("prepared")) {
        return rule.leveledLimit;
    }
    return 0;
}

bool CharacterSheet::setSpellPreparedState(const QString &className, const Spell &spell, bool prepared)
{
    if (!m_character || spell.name.trimmed().isEmpty() || spell.level <= 0) {
        return false;
    }

    QList<Spell> updatedSpells = m_character->spells;
    const auto matchesTarget = [&](const Spell &entry) {
        return spellMatchesExact(entry, className, spell.name, spell.level);
    };

    bool alreadyPrepared = false;
    int preparedCount = 0;
    for (const Spell &entry : updatedSpells) {
        if (spellBelongsToClass(entry, className) && entry.level > 0) {
            ++preparedCount;
        }
        if (matchesTarget(entry)) {
            alreadyPrepared = true;
        }
    }

    if (prepared) {
        if (alreadyPrepared) {
            return true;
        }

        const int limit = preparedSpellLimitForClass(className);
        if (limit > 0 && preparedCount >= limit) {
            QMessageBox::warning(
                this,
                QStringLiteral("Подготовка заклинаний"),
                QStringLiteral("Для класса «%1» уже достигнут максимум подготовленных заклинаний: %2.")
                    .arg(className)
                    .arg(limit));
            return false;
        }

        Spell entry = spell;
        entry.selectionClass = className;
        updatedSpells << entry;
    } else {
        updatedSpells.erase(std::remove_if(updatedSpells.begin(), updatedSpells.end(), matchesTarget), updatedSpells.end());
    }

    m_character->spells = uniqueSortedSpells(updatedSpells);
    updateFromCharacter();
    emit characterUpdated();
    return true;
}

void CharacterSheet::updateSpellcastingSection()
{
    if (!m_character) {
        clearExtendedSections();
        return;
    }

    if (m_allSpells.isEmpty()) {
        m_allSpells = DatabaseManager::instance().getAllSpells();
    }

    const QStringList classes = spellcastingClasses();
    const QString previousClassFilter = spellClassFilter->currentData().toString();
    const QVariant previousLevelData = spellLevelFilter->currentData();
    const int previousLevelFilter = previousLevelData.isValid() ? previousLevelData.toInt() : -1;
    const QString previousStateFilter = spellStateFilter->currentData().toString();

    {
        QSignalBlocker blocker(spellClassFilter);
        spellClassFilter->clear();
        spellClassFilter->addItem(QStringLiteral("Все классы"), QString());
        for (const QString &className : classes) {
            spellClassFilter->addItem(className, className);
        }
        const int classIndex = spellClassFilter->findData(previousClassFilter);
        spellClassFilter->setCurrentIndex(classIndex >= 0 ? classIndex : 0);
    }

    {
        QSignalBlocker blocker(spellLevelFilter);
        const int levelIndex = spellLevelFilter->findData(previousLevelFilter);
        spellLevelFilter->setCurrentIndex(levelIndex >= 0 ? levelIndex : 0);
    }

    {
        QSignalBlocker blocker(spellStateFilter);
        const int stateIndex = spellStateFilter->findData(previousStateFilter);
        spellStateFilter->setCurrentIndex(stateIndex >= 0 ? stateIndex : 0);
    }

    if (classes.isEmpty()) {
        spellcastingSummaryLabel->setText(QStringLiteral("У персонажа нет магических классов или выбор заклинаний ещё не завершён."));
        rebuildSpellSlotEditors();
        rebuildSpellTree();
        return;
    }

    QStringList summaryLines;
    for (const QString &className : classes) {
        const int classLevel = m_character->classLevels.value(className, 0);
        const SpellSelectionRule rule = CharacterProgressionRules::instance().spellSelectionRuleForClass(m_character, className, classLevel);
        const QList<Spell> classKnownSpells = knownSpellsForClass(className);
        int cantripCount = 0;
        int leveledCount = 0;
        for (const Spell &spell : classKnownSpells) {
            if (spell.level == 0) {
                ++cantripCount;
            } else {
                ++leveledCount;
            }
        }

        if (rule.usesSpellbook) {
            summaryLines << QStringLiteral("%1: заговоры %2/%3, книга %4/%5, подготовлено %6/%7")
                .arg(className)
                .arg(cantripCount)
                .arg(rule.cantripLimit)
                .arg(spellbookForClass(className).size())
                .arg(rule.leveledLimit)
                .arg(leveledCount)
                .arg(rule.preparedLimit);
        } else if (rule.mode == QStringLiteral("prepared")) {
            summaryLines << QStringLiteral("%1: заговоры %2/%3, подготовлено %4/%5")
                .arg(className)
                .arg(cantripCount)
                .arg(rule.cantripLimit)
                .arg(leveledCount)
                .arg(rule.leveledLimit);
        } else {
            summaryLines << QStringLiteral("%1: заговоры %2/%3, известных %4/%5")
                .arg(className)
                .arg(cantripCount)
                .arg(rule.cantripLimit)
                .arg(leveledCount)
                .arg(rule.leveledLimit);
        }

        if (!rule.note.trimmed().isEmpty()) {
            summaryLines << QStringLiteral("  %1").arg(rule.note.trimmed());
        }
    }
    spellcastingSummaryLabel->setText(summaryLines.join(QStringLiteral("\n")));

    rebuildSpellSlotEditors();
    rebuildSpellTree();
}

void CharacterSheet::rebuildSpellSlotEditors()
{
    clearLayout(spellSlotsLayout);

    if (!m_character) {
        return;
    }

    bool hasAnySlots = false;
    for (const QString &className : spellcastingClasses()) {
        const QMap<int, int> maximums = spellSlotMaximumsForClass(className);
        if (maximums.isEmpty()) {
            continue;
        }

        hasAnySlots = true;
        QGroupBox *group = new QGroupBox(QStringLiteral("Ячейки: %1").arg(className), spellSlotsContainer);
        QGridLayout *groupLayout = new QGridLayout(group);

        int row = 0;
        for (auto it = maximums.begin(); it != maximums.end(); ++it, ++row) {
            const int slotLevel = it.key();
            const int maximum = it.value();

            QLabel *slotLabel = new QLabel(spellLevelLabel(slotLevel), group);
            QLabel *maximumLabel = new QLabel(QStringLiteral("Макс: %1").arg(maximum), group);
            QSpinBox *currentSpin = new QSpinBox(group);
            currentSpin->setRange(0, maximum);
            currentSpin->setValue(spellSlotCurrentValue(className, slotLevel, maximum));

            groupLayout->addWidget(slotLabel, row, 0);
            groupLayout->addWidget(maximumLabel, row, 1);
            groupLayout->addWidget(new QLabel(QStringLiteral("Текущие:"), group), row, 2);
            groupLayout->addWidget(currentSpin, row, 3);

            connect(currentSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, className, slotLevel, maximum](int value) {
                if (!m_character || m_updatingUi) {
                    return;
                }

                m_character->spellSlotCurrent[spellSlotKey(className, slotLevel)] = qBound(0, value, maximum);
                rebuildSpellTree();
                emit characterUpdated();
            });
        }

        if (normalizedName(className) == normalizedName(QStringLiteral("Колдун"))) {
            QLabel *noteLabel = new QLabel(QStringLiteral("Для колдуна здесь отображаются ячейки Магии договора."), group);
            noteLabel->setWordWrap(true);
            groupLayout->addWidget(noteLabel, row, 0, 1, 4);
        }

        spellSlotsLayout->addWidget(group);
    }

    if (!hasAnySlots) {
        QLabel *emptyLabel = new QLabel(QStringLiteral("На текущих уровнях у персонажа пока нет расходуемых ячеек заклинаний."), spellSlotsContainer);
        emptyLabel->setWordWrap(true);
        spellSlotsLayout->addWidget(emptyLabel);
    }

    spellSlotsLayout->addStretch();
}

void CharacterSheet::rebuildSpellTree()
{
    if (!spellTree) {
        return;
    }

    QSignalBlocker blocker(spellTree);
    m_updatingSpellTree = true;
    spellTree->clear();

    if (!m_character) {
        m_updatingSpellTree = false;
        return;
    }

    const QString classFilter = spellClassFilter->currentData().toString();
    const QVariant levelFilterData = spellLevelFilter->currentData();
    const int levelFilter = levelFilterData.isValid() ? levelFilterData.toInt() : -1;
    const QString stateFilter = spellStateFilter->currentData().toString();
    const QString searchNeedle = normalizedName(spellSearchEdit->text());

    QMap<QString, QTreeWidgetItem *> classItems;
    QMap<QString, QMap<int, QTreeWidgetItem *>> levelItems;
    QTreeWidgetItem *firstSpellItem = nullptr;

    const auto ensureClassItem = [&](const QString &className) {
        if (!classItems.contains(className)) {
            QTreeWidgetItem *classItem = new QTreeWidgetItem(spellTree);
            classItem->setText(1, className);
            classItem->setExpanded(true);
            classItems.insert(className, classItem);
        }
        return classItems.value(className);
    };

    const auto ensureLevelItem = [&](const QString &className, int spellLevel) {
        if (!levelItems[className].contains(spellLevel)) {
            QTreeWidgetItem *levelItem = new QTreeWidgetItem(ensureClassItem(className));
            levelItem->setText(1, spellLevelLabel(spellLevel));
            levelItem->setExpanded(true);
            levelItems[className].insert(spellLevel, levelItem);
        }
        return levelItems[className].value(spellLevel);
    };

    const auto appendSpellItem = [&](const QString &className, const Spell &spell, const QString &statusLabel, bool prepared, bool mutablePrepared) {
        const bool castable = prepared && canCastSpellForClass(className, spell.level);
        const bool active = prepared || (!mutablePrepared && statusLabel != QStringLiteral("В книге"));
        const QString castState = spell.level == 0
            ? QStringLiteral("Готово")
            : (!prepared ? QStringLiteral("Не подготовлено") : (castable ? QStringLiteral("Готово") : QStringLiteral("Нет ячеек")));

        if (!classFilter.isEmpty() && normalizedName(classFilter) != normalizedName(className)) {
            return;
        }
        if (levelFilter >= 0 && spell.level != levelFilter) {
            return;
        }

        const QString searchableText = normalizedName(
            QStringLiteral("%1 %2 %3 %4")
                .arg(spell.name)
                .arg(spell.school)
                .arg(spell.description)
                .arg(className));
        if (!searchNeedle.isEmpty() && !searchableText.contains(searchNeedle)) {
            return;
        }

        if (stateFilter == QStringLiteral("active") && !active) {
            return;
        }
        if (stateFilter == QStringLiteral("castable") && castState != QStringLiteral("Готово")) {
            return;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(ensureLevelItem(className, spell.level));
        item->setText(1, spell.name);
        item->setText(2, spellLevelLabel(spell.level));
        item->setText(3, className);
        item->setText(4, castState);

        item->setData(0, Qt::UserRole, className);
        item->setData(0, Qt::UserRole + 1, spell.name);
        item->setData(0, Qt::UserRole + 2, spell.level);
        item->setData(0, Qt::UserRole + 3, mutablePrepared);
        item->setData(0, Qt::UserRole + 4, statusLabel);
        item->setData(0, Qt::UserRole + 5, spell.school);
        item->setData(0, Qt::UserRole + 6, spell.description);
        item->setData(0, Qt::UserRole + 7, spell.castingTime);
        item->setData(0, Qt::UserRole + 8, spell.range);
        item->setData(0, Qt::UserRole + 9, spell.components);
        item->setData(0, Qt::UserRole + 10, spell.duration);
        item->setData(0, Qt::UserRole + 11, spell.upper);
        item->setData(0, Qt::UserRole + 12, spell.source);

        if (mutablePrepared) {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, prepared ? Qt::Checked : Qt::Unchecked);
        } else {
            item->setText(0, statusLabel);
        }

        if (!firstSpellItem) {
            firstSpellItem = item;
        }
    };

    for (const QString &className : spellcastingClasses()) {
        const int classLevel = m_character->classLevels.value(className, 0);
        const SpellSelectionRule rule = CharacterProgressionRules::instance().spellSelectionRuleForClass(m_character, className, classLevel);
        const QList<Spell> classKnownSpells = knownSpellsForClass(className);

        for (const Spell &spell : classKnownSpells) {
            if (spell.level == 0) {
                appendSpellItem(className, spell, QStringLiteral("Заговор"), true, false);
            }
        }

        if (rule.usesSpellbook) {
            const QList<Spell> classSpellbook = spellbookForClass(className);
            for (const Spell &spell : classSpellbook) {
                const bool prepared = std::any_of(classKnownSpells.begin(), classKnownSpells.end(), [&](const Spell &entry) {
                    return entry.level > 0 && spellMatchesExact(entry, className, spell.name, spell.level);
                });
                appendSpellItem(className, spell, prepared ? QStringLiteral("Подготовлено") : QStringLiteral("В книге"), prepared, true);
            }
            continue;
        }

        if (rule.mode == QStringLiteral("prepared")) {
            const QList<Spell> availablePreparedSpells = availablePreparedSpellsForClass(className, classLevel, rule.maxSpellLevel);
            for (const Spell &spell : availablePreparedSpells) {
                const bool prepared = std::any_of(classKnownSpells.begin(), classKnownSpells.end(), [&](const Spell &entry) {
                    return entry.level > 0 && spellMatchesExact(entry, className, spell.name, spell.level);
                });
                appendSpellItem(className, spell, prepared ? QStringLiteral("Подготовлено") : QStringLiteral("Не подготовлено"), prepared, true);
            }
            continue;
        }

        for (const Spell &spell : classKnownSpells) {
            if (spell.level > 0) {
                appendSpellItem(className, spell, QStringLiteral("Известно"), true, false);
            }
        }
    }

    spellTree->expandToDepth(1);
    if (firstSpellItem) {
        spellTree->setCurrentItem(firstSpellItem);
        updateSpellDetails(firstSpellItem);
    } else {
        updateSpellDetails(nullptr);
    }

    m_updatingSpellTree = false;
}

void CharacterSheet::updateSpellDetails(QTreeWidgetItem *item)
{
    if (!item || item->data(0, Qt::UserRole + 1).toString().trimmed().isEmpty()) {
        spellDetailsTitleLabel->setText(QStringLiteral("Выберите заклинание"));
        spellDetailsMetaLabel->clear();
        spellDetailsText->setPlainText(QStringLiteral("Здесь будет описание выбранного заклинания, его текущий статус и информация для каста."));
        return;
    }

    const QString className = item->data(0, Qt::UserRole).toString();
    const QString spellName = item->data(0, Qt::UserRole + 1).toString();
    const int spellLevel = item->data(0, Qt::UserRole + 2).toInt();
    const QString statusLabel = item->data(0, Qt::UserRole + 4).toString();
    const QString school = item->data(0, Qt::UserRole + 5).toString();
    const QString description = item->data(0, Qt::UserRole + 6).toString();
    const QString castingTime = item->data(0, Qt::UserRole + 7).toString();
    const QString spellRange = item->data(0, Qt::UserRole + 8).toString();
    const QString components = item->data(0, Qt::UserRole + 9).toString();
    const QString duration = item->data(0, Qt::UserRole + 10).toString();
    const QString upper = item->data(0, Qt::UserRole + 11).toString();
    const QString source = item->data(0, Qt::UserRole + 12).toString();

    spellDetailsTitleLabel->setText(spellName);
    spellDetailsMetaLabel->setText(
        QStringLiteral("Класс: %1 | Уровень: %2 | Школа: %3 | Статус: %4 | Каст: %5")
            .arg(className)
            .arg(spellLevelLabel(spellLevel))
            .arg(school.isEmpty() ? QStringLiteral("—") : school)
            .arg(statusLabel.isEmpty() ? QStringLiteral("—") : statusLabel)
            .arg(item->text(4)));

    QStringList details;
    details << QStringLiteral("Время накладывания: %1").arg(castingTime.isEmpty() ? QStringLiteral("—") : castingTime);
    details << QStringLiteral("Дистанция: %1").arg(spellRange.isEmpty() ? QStringLiteral("—") : spellRange);
    details << QStringLiteral("Компоненты: %1").arg(components.isEmpty() ? QStringLiteral("—") : components);
    details << QStringLiteral("Длительность: %1").arg(duration.isEmpty() ? QStringLiteral("—") : duration);
    if (!source.trimmed().isEmpty()) {
        details << QStringLiteral("Источник: %1").arg(source);
    }
    if (!description.trimmed().isEmpty()) {
        details << QStringLiteral("\n%1").arg(description);
    }
    if (!upper.trimmed().isEmpty()) {
        details << QStringLiteral("\nНакладывание более высокой ячейкой:\n%1").arg(upper);
    }
    spellDetailsText->setPlainText(details.join(QStringLiteral("\n")));
}

void CharacterSheet::updateFeatureSection()
{
    if (!m_character) {
        return;
    }

    languagesValueLabel->setText(joinOrDash(m_character->languages));
    skillsValueLabel->setText(joinOrDash(m_character->skillProficiencies));
    toolsValueLabel->setText(joinOrDash(m_character->toolProficiencies));
    savesValueLabel->setText(joinOrDash(m_character->savingThrowProficiencies));
    armorValueLabel->setText(joinOrDash(m_character->armorProficiencies));
    weaponsValueLabel->setText(joinOrDash(m_character->weaponProficiencies));

    inventoryList->clear();
    for (const Item &item : m_character->inventory) {
        if (item.name.trimmed().isEmpty()) {
            continue;
        }
        QListWidgetItem *entry = new QListWidgetItem(
            item.quantity > 1
                ? QStringLiteral("%1 x%2").arg(item.name).arg(item.quantity)
                : item.name,
            inventoryList);
        entry->setToolTip(item.description);
    }
    if (inventoryList->count() == 0) {
        inventoryList->addItem(QStringLiteral("Инвентарь пуст."));
    }

    attacksList->clear();
    for (const QString &attack : m_character->attacks) {
        if (!attack.trimmed().isEmpty()) {
            attacksList->addItem(attack);
        }
    }
    if (attacksList->count() == 0) {
        attacksList->addItem(QStringLiteral("Атаки не заполнены."));
    }

    rebuildFeatureTree();
}

void CharacterSheet::rebuildFeatureTree()
{
    QSignalBlocker blocker(featuresTree);
    featuresTree->clear();

    if (!m_character) {
        return;
    }

    const auto addFeatureItem = [&](QTreeWidgetItem *parent, const QString &source, const QString &title, const QString &details) {
        QTreeWidgetItem *item = new QTreeWidgetItem(parent);
        item->setText(0, source);
        item->setText(1, title);
        item->setData(0, Qt::UserRole, details);
        return item;
    };

    QTreeWidgetItem *firstLeaf = nullptr;

    const QMap<QString, QString> displayableRaceTraits = displayableRaceFeatures(m_character->traits, m_character->level);
    if (!displayableRaceTraits.isEmpty()) {
        QTreeWidgetItem *raceRoot = new QTreeWidgetItem(featuresTree, {QStringLiteral("Раса"), m_character->race()});
        raceRoot->setExpanded(true);
        for (auto it = displayableRaceTraits.begin(); it != displayableRaceTraits.end(); ++it) {
            QTreeWidgetItem *item = addFeatureItem(raceRoot, QStringLiteral("Раса"), it.key(), it.value());
            if (!firstLeaf) {
                firstLeaf = item;
            }
        }
    }

    if (!m_character->background.trimmed().isEmpty()) {
        QTreeWidgetItem *backgroundRoot = new QTreeWidgetItem(featuresTree, {QStringLiteral("Предыстория"), m_character->background});
        backgroundRoot->setExpanded(true);
        if (!m_character->backgroundDescription.trimmed().isEmpty()) {
            QTreeWidgetItem *item = addFeatureItem(backgroundRoot, QStringLiteral("Предыстория"), QStringLiteral("Описание"), m_character->backgroundDescription);
            if (!firstLeaf) {
                firstLeaf = item;
            }
        }
        if (!m_character->backgroundFeatureName.trimmed().isEmpty()) {
            QTreeWidgetItem *item = addFeatureItem(backgroundRoot, QStringLiteral("Предыстория"), m_character->backgroundFeatureName, m_character->backgroundFeatureDescription);
            if (!firstLeaf) {
                firstLeaf = item;
            }
        }
    }

    if (!m_character->featDescriptions.isEmpty()) {
        QTreeWidgetItem *featRoot = new QTreeWidgetItem(featuresTree, {QStringLiteral("Черты"), QStringLiteral("Полученные черты")});
        featRoot->setExpanded(true);
        for (auto it = m_character->featDescriptions.begin(); it != m_character->featDescriptions.end(); ++it) {
            QTreeWidgetItem *item = addFeatureItem(featRoot, QStringLiteral("Черта"), it.key(), it.value());
            if (!firstLeaf) {
                firstLeaf = item;
            }
        }
    }

    if (!m_character->abilityScoreImprovementLog.isEmpty()) {
        QTreeWidgetItem *asiRoot = new QTreeWidgetItem(featuresTree, {QStringLiteral("Развитие"), QStringLiteral("Повышения характеристик")});
        asiRoot->setExpanded(true);
        for (int index = 0; index < m_character->abilityScoreImprovementLog.size(); ++index) {
            QTreeWidgetItem *item = addFeatureItem(
                asiRoot,
                QStringLiteral("Развитие"),
                QStringLiteral("Повышение характеристик %1").arg(index + 1),
                m_character->abilityScoreImprovementLog.at(index));
            if (!firstLeaf) {
                firstLeaf = item;
            }
        }
    }

    const QMap<QString, Class> classInfo = classDefinitions();
    const QStringList classNames = m_character->classOrder.isEmpty() ? m_character->classLevels.keys() : m_character->classOrder;
    if (!classNames.isEmpty()) {
        QTreeWidgetItem *classRoot = new QTreeWidgetItem(featuresTree, {QStringLiteral("Классы"), QStringLiteral("Классовые умения")});
        classRoot->setExpanded(true);
        for (const QString &className : classNames) {
            const int classLevel = m_character->classLevels.value(className, 0);
            if (classLevel <= 0) {
                continue;
            }

            const Class cls = classInfo.value(className);
            const SpellSelectionRule rule = CharacterProgressionRules::instance().spellSelectionRuleForClass(m_character, className, classLevel);

            QTreeWidgetItem *classItem = new QTreeWidgetItem(classRoot, {QStringLiteral("Класс"), QStringLiteral("%1 %2").arg(className).arg(classLevel)});
            classItem->setExpanded(true);

            QStringList summaryDetails;
            summaryDetails << QStringLiteral("Уровень класса: %1").arg(classLevel);
            if (!cls.description.trimmed().isEmpty()) {
                summaryDetails << QStringLiteral("\n%1").arg(cls.description);
            }
            if (cls.hitDie > 0) {
                summaryDetails << QStringLiteral("\nКость хитов: 1d%1").arg(cls.hitDie);
            }
            if (!cls.primaryAbility.trimmed().isEmpty()) {
                summaryDetails << QStringLiteral("Основная характеристика: %1").arg(cls.primaryAbility);
            }
            if (!cls.savingThrowProficiencies.isEmpty()) {
                summaryDetails << QStringLiteral("Спасброски: %1").arg(cls.savingThrowProficiencies.join(QStringLiteral(", ")));
            }
            if (!cls.armorProficiencies.isEmpty()) {
                summaryDetails << QStringLiteral("Доспехи: %1").arg(cls.armorProficiencies.join(QStringLiteral(", ")));
            }
            if (!cls.weaponProficiencies.isEmpty()) {
                summaryDetails << QStringLiteral("Оружие: %1").arg(cls.weaponProficiencies.join(QStringLiteral(", ")));
            }
            if (!cls.toolProficiencies.isEmpty()) {
                summaryDetails << QStringLiteral("Инструменты: %1").arg(cls.toolProficiencies.join(QStringLiteral(", ")));
            }
            if (cls.skillChoiceCount > 0 && !cls.skillChoices.isEmpty()) {
                summaryDetails << QStringLiteral("Навыки на выбор: %1 из %2").arg(cls.skillChoiceCount).arg(cls.skillChoices.join(QStringLiteral(", ")));
            }
            if (!cls.multiclassRequirement.trimmed().isEmpty()) {
                summaryDetails << QStringLiteral("Мультиклассирование: %1").arg(cls.multiclassRequirement);
            }
            if (!cls.multiclassProficiencies.isEmpty()) {
                summaryDetails << QStringLiteral("Владения при мультиклассе: %1").arg(cls.multiclassProficiencies.join(QStringLiteral(", ")));
            }
            if (!cls.multiclassSpellcastingNote.trimmed().isEmpty()) {
                summaryDetails << QStringLiteral("Магия при мультиклассе: %1").arg(cls.multiclassSpellcastingNote);
            }
            if (rule.isValid()) {
                summaryDetails << QStringLiteral("Макс. круг заклинаний: %1").arg(rule.maxSpellLevel);
                if (!rule.note.trimmed().isEmpty()) {
                    summaryDetails << QStringLiteral("Примечание по магии: %1").arg(rule.note.trimmed());
                }
            }

            QTreeWidgetItem *summaryItem = addFeatureItem(classItem, QStringLiteral("Класс"), QStringLiteral("Сводка"), summaryDetails.join(QStringLiteral("\n")));
            if (!firstLeaf) {
                firstLeaf = summaryItem;
            }

            if (!cls.progression.isEmpty()) {
                QTreeWidgetItem *progressionRoot = new QTreeWidgetItem(classItem, {QStringLiteral("Прогрессия"), QStringLiteral("Что получено по уровням")});
                progressionRoot->setExpanded(true);
                for (const ClassLevelProgression &entry : cls.progression) {
                    if (entry.level <= 0 || entry.level > classLevel) {
                        continue;
                    }

                    QStringList entryDetails;
                    entryDetails << QStringLiteral("Уровень класса: %1").arg(entry.level);
                    entryDetails << QStringLiteral("Бонус мастерства: %1").arg(signedValue(entry.proficiencyBonus));
                    if (!entry.features.isEmpty()) {
                        entryDetails << QStringLiteral("Полученные умения: %1").arg(entry.features.join(QStringLiteral(", ")));
                    }

                    QTreeWidgetItem *item = addFeatureItem(
                        progressionRoot,
                        QStringLiteral("Уровень %1").arg(entry.level),
                        entry.features.isEmpty() ? QStringLiteral("Новая веха") : entry.features.join(QStringLiteral(", ")),
                        entryDetails.join(QStringLiteral("\n"))
                    );
                    if (!firstLeaf) {
                        firstLeaf = item;
                    }
                }
            }

            QList<ClassSection> availableSections;
            for (const ClassSection &section : cls.featureSections) {
                if (section.title.trimmed().isEmpty()) {
                    continue;
                }
                if (section.levelRequirement > 0 && section.levelRequirement > classLevel) {
                    continue;
                }
                availableSections.append(section);
            }

            if (!availableSections.isEmpty()) {
                QTreeWidgetItem *featuresRoot = new QTreeWidgetItem(classItem, {QStringLiteral("Умения"), QStringLiteral("Подробные классовые умения")});
                featuresRoot->setExpanded(true);
                for (const ClassSection &section : availableSections) {
                    QStringList sectionDetails;
                    if (!section.levelText.trimmed().isEmpty()) {
                        sectionDetails << section.levelText.trimmed();
                    }
                    if (section.optional) {
                        sectionDetails << QStringLiteral("Опциональное умение");
                    }
                    if (!section.description.trimmed().isEmpty()) {
                        sectionDetails << section.description.trimmed();
                    }

                    QTreeWidgetItem *item = addFeatureItem(featuresRoot, QStringLiteral("Умение"), section.title, sectionDetails.join(QStringLiteral("\n\n")));
                    if (!firstLeaf) {
                        firstLeaf = item;
                    }
                }
            }
        }
    }

    if (!firstLeaf) {
        QTreeWidgetItem *placeholder = new QTreeWidgetItem(featuresTree, {QStringLiteral("Нет данных"), QStringLiteral("Особенности ещё не заполнены")});
        placeholder->setData(0, Qt::UserRole, QStringLiteral("После выбора расы, классов, черт и заклинаний здесь появятся подробные блоки с описаниями."));
        firstLeaf = placeholder;
    }

    featuresTree->expandAll();
    featuresTree->setCurrentItem(firstLeaf);
    updateFeatureDetails(firstLeaf);
}

void CharacterSheet::updateFeatureDetails(QTreeWidgetItem *item)
{
    if (!item) {
        featureDetailsText->setPlainText(QStringLiteral("Выберите особенность в левой части, чтобы увидеть полное описание."));
        return;
    }

    const QString details = item->data(0, Qt::UserRole).toString();
    if (details.trimmed().isEmpty()) {
        featureDetailsText->setPlainText(QStringLiteral("Для этого блока подробное описание отсутствует."));
        return;
    }

    featureDetailsText->setPlainText(details);
}

void CharacterSheet::onFieldChanged()
{
    if (!m_character || m_updatingUi) {
        return;
    }

    const QString trimmedName = ui->nameEdit->text().trimmed();
    if (!trimmedName.isEmpty()) {
        m_character->setName(trimmedName);
    }

    m_character->background = ui->backgroundEdit->text().trimmed();
    m_character->personalHistory = historyEdit->toPlainText();
    m_character->appearance = ui->appearanceEdit->toPlainText();
    m_character->age = ui->ageEdit->text().trimmed();
    m_character->height = ui->heightEdit->text().trimmed();
    m_character->weight = ui->weightEdit->text().trimmed();
    m_character->skin = ui->skinEdit->text().trimmed();
    m_character->hair = ui->hairEdit->text().trimmed();
    m_character->alignment = ui->alignmentCombo->currentText();

    bool ok = false;
    const int newCurrentHp = ui->hpCurrentEdit->text().toInt(&ok);
    if (ok) {
        m_character->currentHp = qBound(0, newCurrentHp, m_character->maxHp);
    }

    const int newTempHp = ui->hpTempEdit->text().toInt(&ok);
    if (ok) {
        m_character->tempHp = qMax(0, newTempHp);
    }

    m_character->strength = scoreFromFieldText(ui->strEdit->text(), m_character->strength);
    m_character->dexterity = scoreFromFieldText(ui->dexEdit->text(), m_character->dexterity);
    m_character->constitution = scoreFromFieldText(ui->conEdit->text(), m_character->constitution);
    m_character->intelligence = scoreFromFieldText(ui->intEdit->text(), m_character->intelligence);
    m_character->wisdom = scoreFromFieldText(ui->wisEdit->text(), m_character->wisdom);
    m_character->charisma = scoreFromFieldText(ui->chaEdit->text(), m_character->charisma);

    m_character->recalculateDerivedStats(false);
    updateFromCharacter();

    {
        QSignalBlocker blocker(ui->nameEdit);
        ui->nameEdit->setText(m_character->name());
    }
    {
        QSignalBlocker blocker(ui->hpCurrentEdit);
        ui->hpCurrentEdit->setText(QString::number(m_character->currentHp));
    }
    {
        QSignalBlocker blocker(ui->hpTempEdit);
        ui->hpTempEdit->setText(QString::number(m_character->tempHp));
    }

    emit characterUpdated();
}

void CharacterSheet::onSpellFiltersChanged()
{
    if (m_updatingUi) {
        return;
    }
    rebuildSpellTree();
}

void CharacterSheet::onSpellItemChanged(QTreeWidgetItem *item, int column)
{
    if (!item || column != 0 || !m_character || m_updatingUi || m_updatingSpellTree) {
        return;
    }

    if (!item->data(0, Qt::UserRole + 3).toBool()) {
        return;
    }

    const QString className = item->data(0, Qt::UserRole).toString();
    const QString spellName = item->data(0, Qt::UserRole + 1).toString();
    const int spellLevel = item->data(0, Qt::UserRole + 2).toInt();
    const Spell spell = findSpellForClass(className, spellName, spellLevel);
    if (spell.name.trimmed().isEmpty()) {
        updateFromCharacter();
        return;
    }

    if (!setSpellPreparedState(className, spell, item->checkState(0) == Qt::Checked)) {
        updateFromCharacter();
    }
}

void CharacterSheet::onSpellSelectionChanged()
{
    updateSpellDetails(spellTree ? spellTree->currentItem() : nullptr);
}

void CharacterSheet::onFeatureSelectionChanged()
{
    updateFeatureDetails(featuresTree ? featuresTree->currentItem() : nullptr);
}