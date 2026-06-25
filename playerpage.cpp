#include "playerpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QListWidget>
#include <QRandomGenerator>
#include <QDebug>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QRegularExpression>
#include <QLineEdit>
#include <QScrollArea>
#include <QSplitter>
#include <QTextEdit>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QListWidgetItem>
#include <QUrl>
#include <QCoreApplication>
#include <algorithm>
#include "character.h"
#include "characterprogressionrules.h"
#include "background.h"
#include "databasemanager.h"
#include "feat.h"
#include "spellbookwidget.h"
#include "itembookwidget.h"

namespace {

const QStringList &knownSkillNames()
{
    static const QStringList skills = {
        QStringLiteral("Атлетика"),
        QStringLiteral("Акробатика"),
        QStringLiteral("Ловкость рук"),
        QStringLiteral("Скрытность"),
        QStringLiteral("Магия"),
        QStringLiteral("История"),
        QStringLiteral("Анализ"),
        QStringLiteral("Природа"),
        QStringLiteral("Религия"),
        QStringLiteral("Уход за животными"),
        QStringLiteral("Проницательность"),
        QStringLiteral("Медицина"),
        QStringLiteral("Внимательность"),
        QStringLiteral("Выживание"),
        QStringLiteral("Обман"),
        QStringLiteral("Запугивание"),
        QStringLiteral("Выступление"),
        QStringLiteral("Убеждение"),
        QStringLiteral("Восприятие")
    };

    return skills;
}

QString resolveMaterialsDirPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::current().absoluteFilePath(QStringLiteral("materials")),
        QDir(appDir).absoluteFilePath(QStringLiteral("materials")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../materials")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../materials")),
        QDir(appDir).absoluteFilePath(QStringLiteral("../../../materials"))
    };

    for (const QString &candidate : candidates) {
        QDir dir(candidate);
        if (dir.exists()) {
            return dir.absolutePath();
        }
    }

    return QString();
}

bool openMaterialByPath(const QString &path, QWidget *parent)
{
    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    if (!opened) {
        QMessageBox::warning(
            parent,
            QStringLiteral("Справочный материал"),
            QStringLiteral("Не удалось открыть файл:\n%1").arg(QDir::toNativeSeparators(path)));
    }
    return opened;
}

void showReferenceMaterialsDialog(QWidget *parent)
{
    const QString materialsDirPath = resolveMaterialsDirPath();
    if (materialsDirPath.isEmpty()) {
        QMessageBox::warning(
            parent,
            QStringLiteral("Справочный материал"),
            QStringLiteral("Папка materials не найдена."));
        return;
    }

    const QDir materialsDir(materialsDirPath);
    const QStringList filters = {
        QStringLiteral("*.pdf"),
        QStringLiteral("*.doc"),
        QStringLiteral("*.docx"),
        QStringLiteral("*.txt"),
        QStringLiteral("*.md"),
        QStringLiteral("*.rtf")
    };
    const QFileInfoList materials = materialsDir.entryInfoList(
        filters,
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase);

    if (materials.isEmpty()) {
        QMessageBox::information(
            parent,
            QStringLiteral("Справочный материал"),
            QStringLiteral("В папке materials пока нет доступных файлов для чтения."));
        return;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("Справочный материал"));
    dialog.resize(760, 520);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *hintLabel = new QLabel(QStringLiteral("Выберите материал и нажмите «Открыть»:"), &dialog);
    layout->addWidget(hintLabel);

    QListWidget *filesList = new QListWidget(&dialog);
    filesList->setSelectionMode(QAbstractItemView::SingleSelection);
    for (const QFileInfo &info : materials) {
        QListWidgetItem *item = new QListWidgetItem(info.fileName(), filesList);
        item->setData(Qt::UserRole, info.absoluteFilePath());
    }
    filesList->setCurrentRow(0);
    layout->addWidget(filesList, 1);

    QLabel *pathLabel = new QLabel(
        QStringLiteral("Папка: %1").arg(QDir::toNativeSeparators(materialsDir.absolutePath())),
        &dialog);
    pathLabel->setProperty("role", QStringLiteral("muted"));
    pathLabel->setWordWrap(true);
    layout->addWidget(pathLabel);

    QDialogButtonBox *buttons = new QDialogButtonBox(&dialog);
    QPushButton *openButton = buttons->addButton(QStringLiteral("Открыть"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QStringLiteral("Закрыть"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    const auto openSelected = [&dialog, filesList]() {
        QListWidgetItem *current = filesList->currentItem();
        if (!current) {
            QMessageBox::information(
                &dialog,
                QStringLiteral("Справочный материал"),
                QStringLiteral("Выберите файл из списка."));
            return;
        }

        const QString filePath = current->data(Qt::UserRole).toString();
        openMaterialByPath(filePath, &dialog);
    };

    QObject::connect(openButton, &QPushButton::clicked, &dialog, openSelected);
    QObject::connect(filesList, &QListWidget::itemDoubleClicked, &dialog, [openSelected](QListWidgetItem *) {
        openSelected();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.exec();
}

int minimumRequiredLevelFromText(const QString &text)
{
    int minimumLevel = 1;
    bool matched = false;

    const QRegularExpression regex(
        QStringLiteral("(?:начиная\\s+с|с)\\s*(\\d+)\\s*(?:-|–)?\\s*(?:го|й|ого)?\\s+уров"),
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

struct ChoiceEntry {
    ChoiceEntry() = default;

    ChoiceEntry(
        const QString &keyValue,
        const QString &titleValue,
        const QString &detailsValue,
        bool selectedValue = false,
        const QString &categoryValue = QString())
        : key(keyValue),
          title(titleValue),
          details(detailsValue),
          selected(selectedValue),
          category(categoryValue)
    {
    }

    QString key;
    QString title;
    QString details;
    bool selected = false;
    QString category;
};

class SearchableChoiceDialog : public QDialog
{
public:
    SearchableChoiceDialog(
        const QString &title,
        const QString &prompt,
        const QList<ChoiceEntry> &entries,
        bool multiSelect,
                QWidget *parent = nullptr,
                int maxSelections = -1,
                const QString &selectionLimitMessage = QString(),
                const QString &categoryFilterLabel = QString())
        : QDialog(parent),
                    m_multiSelect(multiSelect),
                    m_maxSelections(maxSelections),
                    m_selectionLimitMessage(selectionLimitMessage)
    {
        setWindowTitle(title);
        resize(920, 620);

        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *promptLabel = new QLabel(prompt, this);
        promptLabel->setWordWrap(true);
        layout->addWidget(promptLabel);

        m_searchEdit = new QLineEdit(this);
        m_searchEdit->setPlaceholderText("Поиск...");
        layout->addWidget(m_searchEdit);

        QStringList categories;
        for (const ChoiceEntry &entry : entries) {
            const QString category = entry.category.trimmed();
            if (!category.isEmpty() && !categories.contains(category)) {
                categories << category;
            }
        }

        if (!categories.isEmpty()) {
            m_categoryFilter = new QComboBox(this);
            m_categoryFilter->addItem(categoryFilterLabel.isEmpty() ? QStringLiteral("Все варианты") : categoryFilterLabel, QString());
            for (const QString &category : categories) {
                m_categoryFilter->addItem(category, category);
            }
            layout->addWidget(m_categoryFilter);
        }

        if (m_multiSelect) {
            m_selectionCounterLabel = new QLabel(this);
            layout->addWidget(m_selectionCounterLabel);
        }

        QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
        m_listWidget = new QListWidget(splitter);
        m_detailsView = new QTextEdit(splitter);
        m_detailsView->setReadOnly(true);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 2);
        layout->addWidget(splitter, 1);

        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        layout->addWidget(buttons);

        QListWidgetItem *preferredItem = nullptr;
        for (const ChoiceEntry &entry : entries) {
            QListWidgetItem *item = new QListWidgetItem(entry.title, m_listWidget);
            item->setData(Qt::UserRole, entry.key);
            item->setData(Qt::UserRole + 1, entry.details);
            item->setData(Qt::UserRole + 2, (entry.title + "\n" + entry.details).toLower());
            item->setData(Qt::UserRole + 3, entry.category);

            if (m_multiSelect) {
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(entry.selected ? Qt::Checked : Qt::Unchecked);
            }

            if (entry.selected && !preferredItem) {
                preferredItem = item;
            }
        }

        if (preferredItem) {
            m_listWidget->setCurrentItem(preferredItem);
        } else if (m_listWidget->count() > 0) {
            m_listWidget->setCurrentRow(0);
        }

        connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
            Q_UNUSED(text);
            applyFilters();
        });

        if (m_categoryFilter) {
            connect(m_categoryFilter, &QComboBox::currentTextChanged, this, [this](const QString &) {
                applyFilters();
            });
        }

        connect(m_listWidget, &QListWidget::currentItemChanged, this, [this]() {
            updateDetails();
        });

        if (m_multiSelect) {
            connect(m_listWidget, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
                if (!item || m_updatingChecks) {
                    return;
                }

                if (m_maxSelections >= 0 && item->checkState() == Qt::Checked && checkedItemCount() > m_maxSelections) {
                    m_updatingChecks = true;
                    item->setCheckState(Qt::Unchecked);
                    m_updatingChecks = false;
                    QMessageBox::information(
                        this,
                        windowTitle(),
                        m_selectionLimitMessage.isEmpty()
                            ? QString("Нельзя выбрать больше %1 вариантов.").arg(m_maxSelections)
                            : m_selectionLimitMessage);
                }

                updateSelectionCounter();
            });
        }

        if (!m_multiSelect) {
            connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
                if (item && !item->isHidden()) {
                    accept();
                }
            });
        }

        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            if (!m_multiSelect && !m_listWidget->currentItem()) {
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        applyFilters();
        updateSelectionCounter();
        updateDetails();
    }

    QString selectedKey() const
    {
        QListWidgetItem *item = m_listWidget->currentItem();
        return item ? item->data(Qt::UserRole).toString() : QString();
    }

    QStringList selectedKeys() const
    {
        QStringList keys;
        for (int index = 0; index < m_listWidget->count(); ++index) {
            QListWidgetItem *item = m_listWidget->item(index);
            if (item->checkState() == Qt::Checked) {
                keys << item->data(Qt::UserRole).toString();
            }
        }
        return keys;
    }

private:
    void applyFilters()
    {
        const QString needle = m_searchEdit->text().trimmed().toLower();
        const QString category = m_categoryFilter ? m_categoryFilter->currentData().toString().trimmed() : QString();
        QListWidgetItem *firstVisible = nullptr;

        for (int index = 0; index < m_listWidget->count(); ++index) {
            QListWidgetItem *item = m_listWidget->item(index);
            const bool searchMatch = needle.isEmpty() || item->data(Qt::UserRole + 2).toString().contains(needle);
            const QString itemCategory = item->data(Qt::UserRole + 3).toString().trimmed();
            const bool categoryMatch = category.isEmpty() || itemCategory == category;
            const bool visible = searchMatch && categoryMatch;
            item->setHidden(!visible);
            if (visible && !firstVisible) {
                firstVisible = item;
            }
        }

        if ((!m_listWidget->currentItem() || m_listWidget->currentItem()->isHidden()) && firstVisible) {
            m_listWidget->setCurrentItem(firstVisible);
        }

        updateDetails();
    }

    int checkedItemCount() const
    {
        int count = 0;
        for (int index = 0; index < m_listWidget->count(); ++index) {
            QListWidgetItem *item = m_listWidget->item(index);
            if (item->checkState() == Qt::Checked) {
                ++count;
            }
        }
        return count;
    }

    void updateSelectionCounter()
    {
        if (!m_selectionCounterLabel) {
            return;
        }

        const int count = checkedItemCount();
        if (m_maxSelections >= 0) {
            m_selectionCounterLabel->setText(QString("Выбрано: %1 / %2").arg(count).arg(m_maxSelections));
        } else {
            m_selectionCounterLabel->setText(QString("Выбрано: %1").arg(count));
        }
    }

    void updateDetails()
    {
        QListWidgetItem *item = m_listWidget->currentItem();
        if (!item || item->isHidden()) {
            m_detailsView->clear();
            return;
        }

        m_detailsView->setPlainText(item->data(Qt::UserRole + 1).toString());
    }

    bool m_multiSelect;
    int m_maxSelections = -1;
    QString m_selectionLimitMessage;
    bool m_updatingChecks = false;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_categoryFilter = nullptr;
    QLabel *m_selectionCounterLabel = nullptr;
    QListWidget *m_listWidget = nullptr;
    QTextEdit *m_detailsView = nullptr;
};

// Forward declarations
const QStringList &knownSkillNames();
bool raceTraitAvailableAtLevel(const QString &title, const QString &description, int level);

QStringList uniqueStrings(QStringList values)
{
    for (QString &value : values) {
        value = value.trimmed();
    }
    values.removeAll(QString());
    values.removeDuplicates();
    return values;
}

int sumLevels(const QMap<QString, int> &levels)
{
    int total = 0;
    for (auto it = levels.begin(); it != levels.end(); ++it) {
        total += qMax(0, it.value());
    }
    return total;
}

int rollAbilityScore4d6DropLowest()
{
    QList<int> rolls;
    for (int index = 0; index < 4; ++index) {
        rolls << QRandomGenerator::global()->bounded(1, 7);
    }

    std::sort(rolls.begin(), rolls.end());
    return rolls[1] + rolls[2] + rolls[3];
}

int choiceCountFromText(const QString &text)
{
    const QString lowered = text.toLower();
    if (lowered.contains("С‚СЂРё")) {
        return 3;
    }
    if (lowered.contains("РґРІ") || lowered.contains("2")) {
        return 2;
    }
    return 1;
}

struct LanguagePattern {
    QString canonical;
    QStringList aliases;
};

struct ParsedLanguageEntry {
    QStringList fixedLanguages;
    QStringList choiceOptions;
    bool hasChoice = false;
};

QString normalizedName(QString value);

const QList<LanguagePattern> &knownLanguagePatterns()
{
    static const QList<LanguagePattern> patterns = {
        {QStringLiteral("Общий"), {QStringLiteral("общий")}},
        {QStringLiteral("Дварфский"), {QStringLiteral("дварф")}},
        {QStringLiteral("Эльфийский"), {QStringLiteral("эльф")}},
        {QStringLiteral("Гигантский"), {QStringLiteral("великан"), QStringLiteral("гигант")}},
        {QStringLiteral("Гномий"), {QStringLiteral("гном")}},
        {QStringLiteral("Гоблинский"), {QStringLiteral("гоблин")}},
        {QStringLiteral("Полуросликов"), {QStringLiteral("полурос")}},
        {QStringLiteral("Орочий"), {QStringLiteral("ороч")}},
        {QStringLiteral("Драконий"), {QStringLiteral("дракон")}},
        {QStringLiteral("Инфернальный"), {QStringLiteral("инферн")}},
        {QStringLiteral("Небесный"), {QStringLiteral("небес")}},
        {QStringLiteral("Глубинная речь"), {QStringLiteral("глубинн")}},
        {QStringLiteral("Подземный"), {QStringLiteral("подзем")}},
        {QStringLiteral("Сильван"), {QStringLiteral("сильван")}},
        {QStringLiteral("Первичный"), {QStringLiteral("первич"), QStringLiteral("первород")}},
        {QStringLiteral("Ауран"), {QStringLiteral("ауран")}},
        {QStringLiteral("Акван"), {QStringLiteral("акван")}},
        {QStringLiteral("Игнан"), {QStringLiteral("игнан")}},
        {QStringLiteral("Терран"), {QStringLiteral("терран")}},
        {QStringLiteral("Язык Бездны"), {QStringLiteral("бездн")}},
        {QStringLiteral("Язык Краулов"), {QStringLiteral("краул")}},
        {QStringLiteral("Ведалкенский"), {QStringLiteral("ведалкен")}}
    };

    return patterns;
}

QStringList extractLanguageNames(const QString &text)
{
    const QString lowered = normalizedName(text);
    QStringList languages;
    for (const LanguagePattern &pattern : knownLanguagePatterns()) {
        for (const QString &alias : pattern.aliases) {
            if (lowered.contains(alias)) {
                languages << pattern.canonical;
                break;
            }
        }
    }
    return uniqueStrings(languages);
}

bool languageEntryImpliesChoice(const QString &text)
{
    const QString lowered = normalizedName(text);
    return lowered.contains(QStringLiteral("выберите")) ||
           lowered.contains(QStringLiteral("выбор")) ||
           lowered.contains(QStringLiteral("по вашему выбору")) ||
           lowered.contains(QStringLiteral("на ваш выбор")) ||
           lowered.contains(QStringLiteral(" РёР»Рё ")) ||
           lowered.contains(QStringLiteral("любой"));
}

QStringList explicitLanguageOptions(const QString &text)
{
    const QString lowered = normalizedName(text);
    if (lowered.contains(QStringLiteral("рекоменду"))) {
        return {};
    }

    if (lowered.contains(QStringLiteral("РёР»Рё")) ||
        lowered.contains(QStringLiteral("РёР· ")) ||
        lowered.contains(QStringLiteral("следующ"))) {
        return extractLanguageNames(text);
    }

    return {};
}

ParsedLanguageEntry parseLanguageEntry(const QString &text)
{
    ParsedLanguageEntry parsed;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return parsed;
    }

    const QStringList extractedLanguages = extractLanguageNames(trimmed);
    if (extractedLanguages.isEmpty()) {
        return parsed;
    }

    if (!languageEntryImpliesChoice(trimmed)) {
        parsed.fixedLanguages = extractedLanguages;
        return parsed;
    }

    const QString lowered = normalizedName(trimmed);
    const int orIndex = lowered.lastIndexOf(QStringLiteral(" РёР»Рё "));
    if (orIndex >= 0) {
        QStringList leftLanguages = extractLanguageNames(trimmed.left(orIndex));
        const QStringList rightLanguages = extractLanguageNames(trimmed.mid(orIndex));
        if (!leftLanguages.isEmpty() && !rightLanguages.isEmpty()) {
            const QString pivotLanguage = leftLanguages.takeLast();
            parsed.fixedLanguages = leftLanguages;
            parsed.choiceOptions = uniqueStrings(QStringList{pivotLanguage} + rightLanguages);
            parsed.hasChoice = !parsed.choiceOptions.isEmpty();
            return parsed;
        }
    }

    parsed.fixedLanguages = extractedLanguages;
    parsed.choiceOptions = explicitLanguageOptions(trimmed);
    parsed.hasChoice = !parsed.choiceOptions.isEmpty();
    return parsed;
}

bool isLanguageTraitTitle(const QString &title)
{
    const QString lowered = normalizedName(title);
    return lowered == QStringLiteral("язык") ||
           lowered == QStringLiteral("языки") ||
           lowered.startsWith(QStringLiteral("язык ")) ||
           lowered.startsWith(QStringLiteral("языки "));
}

QStringList raceLanguageEntriesForSelection(const Race &race)
{
    for (auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        if (!isLanguageTraitTitle(it.key())) {
            continue;
        }

        const QString description = it.value().trimmed();
        if (!description.isEmpty() && !explicitLanguageOptions(description).isEmpty()) {
            return {description};
        }
    }

    return race.languages;
}

QString normalizedName(QString value)
{
    return value.simplified().toLower();
}

bool containsByName(const QStringList &values, const QString &needle)
{
    const QString normalizedNeedle = normalizedName(needle);
    for (const QString &value : values) {
        if (normalizedName(value) == normalizedNeedle) {
            return true;
        }
    }
    return false;
}

QStringList allAbilityNames()
{
    return {
        QStringLiteral("Сила"),
        QStringLiteral("Ловкость"),
        QStringLiteral("Телосложение"),
        QStringLiteral("Интеллект"),
        QStringLiteral("Мудрость"),
        QStringLiteral("Харизма")
    };
}

QString spellLevelLabel(int level)
{
    return level == 0 ? QStringLiteral("Заговор") : QStringLiteral("%1 уровень").arg(level);
}

QString spellChoiceTitle(const Spell &spell)
{
    return QString("[%1] %2").arg(spellLevelLabel(spell.level), spell.name);
}

bool isHiddenRaceTraitTitle(const QString &title)
{
    return title.trimmed().startsWith(QStringLiteral("!"));
}

bool isMarkedRaceSpellTraitTitle(const QString &title)
{
    QString cleaned = title.trimmed();
    while (cleaned.startsWith(QStringLiteral("!"))) {
        cleaned.remove(0, 1);
        cleaned = cleaned.trimmed();
    }
    return cleaned.startsWith(QStringLiteral("(заклинание)"), Qt::CaseInsensitive);
}

QString cleanedRaceTraitTitle(QString title)
{
    title = title.trimmed();
    bool changed = true;
    while (changed) {
        changed = false;

        while (title.startsWith(QStringLiteral("!"))) {
            title.remove(0, 1);
            title = title.trimmed();
            changed = true;
        }

        if (title.startsWith(QStringLiteral("(заклинание)"), Qt::CaseInsensitive)) {
            title.remove(0, QStringLiteral("(заклинание)").size());
            title = title.trimmed();
            changed = true;
        }
    }

    return title.simplified();
}

QString cleanedRaceTraitDescription(QString description)
{
    description = description.trimmed();
    description.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]")), QStringLiteral("\\1"));
    return description;
}

QStringList bracketedSpellNames(const QString &text)
{
    QStringList values;
    const QRegularExpression regex(QStringLiteral("\\[([^\\]]+)\\]"));
    QRegularExpressionMatchIterator iterator = regex.globalMatch(text);
    while (iterator.hasNext()) {
        const QString value = iterator.next().captured(1).simplified().trimmed();
        if (!value.isEmpty() && !containsByName(values, value)) {
            values << value;
        }
    }
    return values;
}

QStringList bracketedChoiceValues(const QString &text)
{
    QStringList spellNames;
    const QStringList values = bracketedSpellNames(text);
    for (const QString &value : values) {
        if (!containsByName(spellNames, value)) {
            spellNames << value;
        }
    }
    return spellNames;
}

bool textImpliesChoice(const QString &text)
{
    const QString lowered = normalizedName(text);
    return lowered.contains(QStringLiteral("по вашему выбору")) ||
           lowered.contains(QStringLiteral("выберите")) ||
           lowered.contains(QStringLiteral("один из следующих")) ||
           lowered.contains(QStringLiteral("одним из следующих")) ||
           lowered.contains(QStringLiteral("одну из следующих")) ||
           lowered.contains(QStringLiteral("двух из следующих")) ||
           lowered.contains(QStringLiteral("двумя из следующих")) ||
           lowered.contains(QStringLiteral("одним из перечисленных")) ||
           lowered.contains(QStringLiteral("двумя из перечисленных"));
}

QStringList itemChoicePool(const QStringList &typeFragments)
{
    QStringList values;
    const QList<Item> items = DatabaseManager::instance().getAllItems();
    for (const Item &item : items) {
        if (item.name.trimmed().isEmpty()) {
            continue;
        }

        const QString itemType = normalizedName(item.type);
        bool matches = typeFragments.isEmpty();
        for (const QString &fragment : typeFragments) {
            if (itemType.contains(normalizedName(fragment))) {
                matches = true;
                break;
            }
        }

        if (!matches || containsByName(values, item.name)) {
            continue;
        }

        values << item.name;
    }

    std::sort(values.begin(), values.end(), [](const QString &left, const QString &right) {
        return left.localeAwareCompare(right) < 0;
    });
    return values;
}

QStringList toolChoicePoolForText(const QString &text)
{
    const QString lowered = normalizedName(text);
    if (lowered.contains(QStringLiteral("музык"))) {
        return itemChoicePool({QStringLiteral("музык")});
    }
    if (lowered.contains(QStringLiteral("ремеслен"))) {
        return itemChoicePool({QStringLiteral("ремеслен")});
    }
    return itemChoicePool({QStringLiteral("инструмент")});
}

QStringList weaponChoicePoolForText(const QString &text)
{
    const QString lowered = normalizedName(text);
    if (lowered.contains(QStringLiteral("воинск"))) {
        return itemChoicePool({QStringLiteral("воинское оружие")});
    }
    if (lowered.contains(QStringLiteral("прост"))) {
        return itemChoicePool({QStringLiteral("простое оружие")});
    }
    return itemChoicePool({QStringLiteral("оружие")});
}

QStringList armorChoicePoolForText(const QString &text)
{
    QStringList values;
    const QString lowered = normalizedName(text);
    const auto appendIfMissing = [&](const QString &value) {
        if (!containsByName(values, value)) {
            values << value;
        }
    };

    if (lowered.contains(QStringLiteral("все доспех"))) {
        appendIfMissing(QStringLiteral("Лёгкие доспехи"));
        appendIfMissing(QStringLiteral("Средние доспехи"));
        appendIfMissing(QStringLiteral("Тяжёлые доспехи"));
    } else {
        if (lowered.contains(QStringLiteral("лёгк")) || lowered.contains(QStringLiteral("легк"))) {
            appendIfMissing(QStringLiteral("Лёгкие доспехи"));
        }
        if (lowered.contains(QStringLiteral("средн"))) {
            appendIfMissing(QStringLiteral("Средние доспехи"));
        }
        if (lowered.contains(QStringLiteral("С‚СЏР¶"))) {
            appendIfMissing(QStringLiteral("Тяжёлые доспехи"));
        }
    }

    if (lowered.contains(QStringLiteral("С‰РёС‚"))) {
        appendIfMissing(QStringLiteral("Щиты"));
    }

    return values;
}

QStringList optionsMatchingPool(const QString &text, const QStringList &pool)
{
    const QString lowered = normalizedName(text);
    QStringList options;
    for (const QString &value : pool) {
        if (lowered.contains(normalizedName(value)) && !containsByName(options, value)) {
            options << value;
        }
    }
    return options;
}

QList<ChoiceEntry> choiceEntriesFromNames(
    const QStringList &names,
    const QString &detailsPrefix = QString(),
    const QString &category = QString())
{
    QList<ChoiceEntry> entries;
    for (const QString &name : names) {
        entries.append({
            name,
            name,
            detailsPrefix.isEmpty() ? name : QStringLiteral("%1: %2").arg(detailsPrefix, name),
            false,
            category
        });
    }
    return entries;
}

bool chooseExactEntries(
    QWidget *parent,
    const QString &title,
    const QString &prompt,
    const QList<ChoiceEntry> &entries,
    int selectionCount,
    QStringList *selectedKeys,
    const QString &selectionLabel = QStringLiteral("вариантов"),
    const QString &categoryFilterLabel = QStringLiteral("Все варианты"))
{
    if (!selectedKeys || selectionCount <= 0 || entries.isEmpty()) {
        return false;
    }

    const bool multiSelect = selectionCount != 1;
    while (true) {
        SearchableChoiceDialog dialog(
            title,
            prompt,
            entries,
            multiSelect,
            parent,
            multiSelect ? selectionCount : -1,
            multiSelect
                ? QStringLiteral("Нельзя выбрать больше %1 %2.").arg(selectionCount).arg(selectionLabel)
                : QString(),
            categoryFilterLabel);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        const QStringList keys = multiSelect
            ? uniqueStrings(dialog.selectedKeys())
            : uniqueStrings(QStringList{dialog.selectedKey()});

        if (keys.size() != selectionCount || containsByName(keys, QString())) {
            QMessageBox::warning(
                parent,
                title,
                QStringLiteral("Нужно выбрать ровно %1 %2.").arg(selectionCount).arg(selectionLabel));
            continue;
        }

        *selectedKeys = keys;
        return true;
    }
}

struct RaceTraitChoiceGrant {
    QString sourceTitle;
    QString sourceDescription;
    QStringList optionTitles;
    int count = 1;
};

QString rawRaceTraitTitleForCleanedTitle(const QMap<QString, QString> &traits, const QString &cleanedTitle)
{
    const QString normalizedTitle = normalizedName(cleanedTitle);
    for (auto it = traits.begin(); it != traits.end(); ++it) {
        if (normalizedName(cleanedRaceTraitTitle(it.key())) == normalizedTitle) {
            return it.key();
        }
    }
    return QString();
}

QList<RaceTraitChoiceGrant> raceTraitChoiceGrants(const Race &race, int level)
{
    QList<RaceTraitChoiceGrant> grants;

    for (auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        const QString rawTitle = it.key().trimmed();
        const QString rawDescription = it.value().trimmed();
        if (!isHiddenRaceTraitTitle(rawTitle) || rawDescription.isEmpty() || isMarkedRaceSpellTraitTitle(rawTitle)) {
            continue;
        }

        const QString cleanedTitle = cleanedRaceTraitTitle(rawTitle);
        if (!raceTraitAvailableAtLevel(cleanedTitle, rawDescription, level) || !textImpliesChoice(rawDescription)) {
            continue;
        }

        QStringList optionTitles;
        for (const QString &referencedTitle : bracketedChoiceValues(rawDescription)) {
            const QString rawOptionTitle = rawRaceTraitTitleForCleanedTitle(race.traits, referencedTitle);
            if (rawOptionTitle.isEmpty() || normalizedName(rawOptionTitle) == normalizedName(rawTitle)) {
                continue;
            }

            const QString cleanedOptionTitle = cleanedRaceTraitTitle(rawOptionTitle);
            if (!cleanedOptionTitle.isEmpty() && !containsByName(optionTitles, cleanedOptionTitle)) {
                optionTitles << cleanedOptionTitle;
            }
        }

        if (optionTitles.isEmpty()) {
            continue;
        }

        grants.append({
            cleanedTitle,
            cleanedRaceTraitDescription(rawDescription),
            optionTitles,
            qMin(qMax(1, choiceCountFromText(rawDescription)), static_cast<int>(optionTitles.size()))
        });
    }

    return grants;
}

bool applyRaceTraitChoices(QWidget *parent, const Race &sourceRace, int level, Race *resolvedRace)
{
    if (!resolvedRace) {
        return false;
    }

    Race race = *resolvedRace;
    const QList<RaceTraitChoiceGrant> grants = raceTraitChoiceGrants(sourceRace, level);
    for (const RaceTraitChoiceGrant &grant : grants) {
        QList<ChoiceEntry> entries;
        for (const QString &optionTitle : grant.optionTitles) {
            const QString rawOptionTitle = rawRaceTraitTitleForCleanedTitle(race.traits, optionTitle);
            const QString details = rawOptionTitle.isEmpty()
                ? optionTitle
                : cleanedRaceTraitDescription(race.traits.value(rawOptionTitle));
            entries.append({
                optionTitle,
                optionTitle,
                details.isEmpty() ? optionTitle : details,
                false,
                QStringLiteral("Расовые особенности")
            });
        }

        if (entries.isEmpty()) {
            continue;
        }

        QStringList selectedTitles;
        if (!chooseExactEntries(
                parent,
                QStringLiteral("Расовая особенность: %1").arg(race.name),
                QStringLiteral("%1\n\n%2")
                    .arg(grant.sourceTitle)
                    .arg(grant.sourceDescription),
                entries,
                grant.count,
                &selectedTitles,
                QStringLiteral("особенностей"),
                QStringLiteral("Все особенности"))) {
            return false;
        }

        for (const QString &optionTitle : grant.optionTitles) {
            if (containsByName(selectedTitles, optionTitle)) {
                continue;
            }

            const QString rawOptionTitle = rawRaceTraitTitleForCleanedTitle(race.traits, optionTitle);
            if (!rawOptionTitle.isEmpty()) {
                race.traits.remove(rawOptionTitle);
            }
        }
    }

    *resolvedRace = race;
    return true;
}

enum class RaceProficiencyChoiceTarget {
    Skill,
    Tool,
    Weapon,
    Armor,
    SkillOrTool
};

struct RaceProficiencyChoiceGrant {
    RaceProficiencyChoiceTarget target = RaceProficiencyChoiceTarget::Skill;
    QString sourceTitle;
    QString sourceDescription;
    QStringList options;
    int count = 1;
};

QList<RaceProficiencyChoiceGrant> raceProficiencyChoiceGrants(const Race &race, int level)
{
    QList<RaceProficiencyChoiceGrant> grants;

    for (auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        const QString rawTitle = it.key().trimmed();
        const QString rawDescription = it.value().trimmed();
        if (rawDescription.isEmpty() || isMarkedRaceSpellTraitTitle(rawTitle)) {
            continue;
        }

        const QString cleanedTitle = cleanedRaceTraitTitle(rawTitle);
        if (!raceTraitAvailableAtLevel(cleanedTitle, rawDescription, level) || !textImpliesChoice(rawDescription)) {
            continue;
        }

        const QString lowered = normalizedName(rawDescription);
        const int choiceCount = qMax(1, choiceCountFromText(rawDescription));
        const QStringList bracketedOptions = bracketedChoiceValues(rawDescription);
        const bool mentionsSkills = lowered.contains(QStringLiteral("навык"));
        const bool mentionsTools = lowered.contains(QStringLiteral("инструмент"));
        const bool mentionsWeapons = lowered.contains(QStringLiteral("оруж"));
        const bool mentionsArmor = lowered.contains(QStringLiteral("доспех")) || lowered.contains(QStringLiteral("щит"));

        if (mentionsSkills && mentionsTools && lowered.contains(QStringLiteral(" РёР»Рё "))) {
            QStringList options;
            for (const QString &skill : knownSkillNames()) {
                if (bracketedOptions.isEmpty()) {
                    options << skill;
                    continue;
                }
                if (containsByName(bracketedOptions, skill)) {
                    options << skill;
                }
            }

            QStringList toolOptions = bracketedOptions.isEmpty()
                ? toolChoicePoolForText(rawDescription)
                : bracketedOptions;
            for (const QString &tool : toolOptions) {
                if (!containsByName(options, tool)) {
                    options << tool;
                }
            }

            grants.append({RaceProficiencyChoiceTarget::SkillOrTool, cleanedTitle, rawDescription, uniqueStrings(options), choiceCount});
            continue;
        }

        if (mentionsSkills) {
            QStringList options = bracketedOptions;
            if (options.isEmpty()) {
                options = optionsMatchingPool(rawDescription, knownSkillNames());
            }
            if (options.isEmpty()) {
                options = knownSkillNames();
            }
            grants.append({RaceProficiencyChoiceTarget::Skill, cleanedTitle, rawDescription, uniqueStrings(options), choiceCount});
        }

        if (mentionsTools) {
            QStringList options = bracketedOptions;
            if (options.isEmpty()) {
                options = optionsMatchingPool(rawDescription, toolChoicePoolForText(rawDescription));
            }
            if (options.isEmpty()) {
                options = toolChoicePoolForText(rawDescription);
            }
            grants.append({RaceProficiencyChoiceTarget::Tool, cleanedTitle, rawDescription, uniqueStrings(options), choiceCount});
        }

        if (mentionsWeapons) {
            QStringList options = bracketedOptions;
            if (options.isEmpty()) {
                options = optionsMatchingPool(rawDescription, weaponChoicePoolForText(rawDescription));
            }
            if (options.isEmpty()) {
                options = weaponChoicePoolForText(rawDescription);
            }
            if (!options.isEmpty()) {
                grants.append({RaceProficiencyChoiceTarget::Weapon, cleanedTitle, rawDescription, uniqueStrings(options), choiceCount});
            }
        }

        if (mentionsArmor) {
            QStringList options = bracketedOptions;
            if (options.isEmpty()) {
                options = armorChoicePoolForText(rawDescription);
            }
            if (!options.isEmpty()) {
                grants.append({RaceProficiencyChoiceTarget::Armor, cleanedTitle, rawDescription, uniqueStrings(options), choiceCount});
            }
        }
    }

    return grants;
}

QStringList newlyAddedValues(const QStringList &currentValues, const QStringList &baseValues)
{
    QStringList added;
    for (const QString &value : currentValues) {
        if (!containsByName(baseValues, value) && !containsByName(added, value)) {
            added << value;
        }
    }
    return added;
}

void removeNewlyAddedOptions(QStringList *currentValues, const QStringList &newlyAdded, const QStringList &options)
{
    if (!currentValues) {
        return;
    }

    QStringList retained;
    for (const QString &value : *currentValues) {
        if (containsByName(newlyAdded, value) && containsByName(options, value)) {
            continue;
        }
        retained << value;
    }
    *currentValues = uniqueStrings(retained);
}

bool applyRaceChoiceBenefits(
    QWidget *parent,
    const Race &race,
    Character *character,
    const QStringList &baseSkills,
    const QStringList &baseTools,
    const QStringList &baseArmor,
    const QStringList &baseWeapons)
{
    if (!character) {
        return false;
    }

    QStringList skills = character->skillProficiencies;
    QStringList tools = character->toolProficiencies;
    QStringList armor = character->armorProficiencies;
    QStringList weapons = character->weaponProficiencies;

    const QStringList addedSkills = newlyAddedValues(skills, baseSkills);
    const QStringList addedTools = newlyAddedValues(tools, baseTools);
    const QStringList addedArmor = newlyAddedValues(armor, baseArmor);
    const QStringList addedWeapons = newlyAddedValues(weapons, baseWeapons);

    const QList<RaceProficiencyChoiceGrant> grants = raceProficiencyChoiceGrants(race, character->level);
    for (const RaceProficiencyChoiceGrant &grant : grants) {
        QList<ChoiceEntry> entries;
        QString selectionLabel = QStringLiteral("вариантов");

        if (grant.target == RaceProficiencyChoiceTarget::Skill) {
            removeNewlyAddedOptions(&skills, addedSkills, grant.options);
            entries = choiceEntriesFromNames(grant.options, QStringLiteral("Навык"), QStringLiteral("Навыки"));
            selectionLabel = QStringLiteral("навыков");
        } else if (grant.target == RaceProficiencyChoiceTarget::Tool) {
            removeNewlyAddedOptions(&tools, addedTools, grant.options);
            entries = choiceEntriesFromNames(grant.options, QStringLiteral("Инструмент"), QStringLiteral("Инструменты"));
            selectionLabel = QStringLiteral("владений инструментами");
        } else if (grant.target == RaceProficiencyChoiceTarget::Weapon) {
            removeNewlyAddedOptions(&weapons, addedWeapons, grant.options);
            entries = choiceEntriesFromNames(grant.options, QStringLiteral("Оружие"), QStringLiteral("Оружие"));
            selectionLabel = QStringLiteral("видов оружия");
        } else if (grant.target == RaceProficiencyChoiceTarget::Armor) {
            removeNewlyAddedOptions(&armor, addedArmor, grant.options);
            entries = choiceEntriesFromNames(grant.options, QStringLiteral("Доспех"), QStringLiteral("Доспехи"));
            selectionLabel = QStringLiteral("видов владения доспехами");
        } else {
            removeNewlyAddedOptions(&skills, addedSkills, grant.options);
            removeNewlyAddedOptions(&tools, addedTools, grant.options);
            for (const QString &option : grant.options) {
                const bool isSkill = containsByName(knownSkillNames(), option);
                entries.append({
                    isSkill ? QStringLiteral("skill:%1").arg(option) : QStringLiteral("tool:%1").arg(option),
                    option,
                    isSkill ? QStringLiteral("Навык: %1").arg(option) : QStringLiteral("Инструмент: %1").arg(option),
                    false,
                    isSkill ? QStringLiteral("Навыки") : QStringLiteral("Инструменты")
                });
            }
            selectionLabel = QStringLiteral("вариантов владения");
        }

        if (entries.isEmpty()) {
            continue;
        }

        QStringList selectedKeys;
        if (!chooseExactEntries(
                parent,
                QStringLiteral("Расовые владения: %1").arg(race.name),
                QStringLiteral("%1\n\n%2").arg(grant.sourceTitle, grant.sourceDescription),
                entries,
                grant.count,
                &selectedKeys,
                selectionLabel,
                QStringLiteral("Все варианты"))) {
            return false;
        }

        if (grant.target == RaceProficiencyChoiceTarget::Skill) {
            skills = uniqueStrings(skills + selectedKeys);
        } else if (grant.target == RaceProficiencyChoiceTarget::Tool) {
            tools = uniqueStrings(tools + selectedKeys);
        } else if (grant.target == RaceProficiencyChoiceTarget::Weapon) {
            weapons = uniqueStrings(weapons + selectedKeys);
        } else if (grant.target == RaceProficiencyChoiceTarget::Armor) {
            armor = uniqueStrings(armor + selectedKeys);
        } else {
            for (const QString &key : selectedKeys) {
                if (key.startsWith(QStringLiteral("skill:"), Qt::CaseInsensitive)) {
                    skills = uniqueStrings(skills + QStringList{key.mid(QStringLiteral("skill:").size())});
                } else if (key.startsWith(QStringLiteral("tool:"), Qt::CaseInsensitive)) {
                    tools = uniqueStrings(tools + QStringList{key.mid(QStringLiteral("tool:").size())});
                }
            }
        }
    }

    character->skillProficiencies = uniqueStrings(skills);
    character->toolProficiencies = uniqueStrings(tools);
    character->armorProficiencies = uniqueStrings(armor);
    character->weaponProficiencies = uniqueStrings(weapons);
    return true;
}

QString racialSpellSelectionOwner(const QString &raceName)
{
    return QStringLiteral("Раса: %1").arg(raceName.trimmed());
}

bool isRaceGrantedSpell(const Spell &spell)
{
    return spell.selectionClass.startsWith(QStringLiteral("Раса:"), Qt::CaseInsensitive);
}

bool isSubclassGrantedSpell(const Spell &spell)
{
    return spell.source.startsWith(QStringLiteral("Подкласс:"), Qt::CaseInsensitive);
}

QList<Spell> filterRaceGrantedSpells(const QList<Spell> &spells)
{
    QList<Spell> filtered;
    for (const Spell &spell : spells) {
        if (isRaceGrantedSpell(spell)) {
            filtered.append(spell);
        }
    }
    return filtered;
}

QList<Spell> filterSubclassGrantedSpells(const QList<Spell> &spells)
{
    QList<Spell> filtered;
    for (const Spell &spell : spells) {
        if (isSubclassGrantedSpell(spell)) {
            filtered.append(spell);
        }
    }
    return filtered;
}

QList<Spell> removeRaceGrantedSpells(const QList<Spell> &spells)
{
    QList<Spell> filtered;
    for (const Spell &spell : spells) {
        if (!isRaceGrantedSpell(spell)) {
            filtered.append(spell);
        }
    }
    return filtered;
}

QList<Spell> removeGrantedSpells(const QList<Spell> &spells)
{
    QList<Spell> filtered;
    for (const Spell &spell : spells) {
        if (!isRaceGrantedSpell(spell) && !isSubclassGrantedSpell(spell)) {
            filtered.append(spell);
        }
    }
    return filtered;
}

QList<Spell> uniqueSpells(const QList<Spell> &spells)
{
    QList<Spell> result;
    QStringList seen;
    for (const Spell &spell : spells) {
        const QString key = QStringLiteral("%1|%2|%3|%4")
            .arg(normalizedName(spell.selectionClass))
            .arg(spell.level)
            .arg(normalizedName(spell.name))
            .arg(normalizedName(spell.source));
        if (seen.contains(key)) {
            continue;
        }
        seen << key;
        result.append(spell);
    }
    return result;
}

QString spellListClassFromTraitText(const QString &traitText)
{
    const QString lowered = normalizedName(traitText);
    struct ClassPattern {
        QString canonicalName;
        QStringList fragments;
    };

    static const QList<ClassPattern> patterns = {
        {QStringLiteral("Бард"), {QStringLiteral("бард")}},
        {QStringLiteral("Жрец"), {QStringLiteral("жрец")}},
        {QStringLiteral("Друид"), {QStringLiteral("друид")}},
        {QStringLiteral("Изобретатель"), {QStringLiteral("изобрет")}},
        {QStringLiteral("Колдун"), {QStringLiteral("колдун")}},
        {QStringLiteral("Паладин"), {QStringLiteral("паладин")}},
        {QStringLiteral("Следопыт"), {QStringLiteral("следопыт")}},
        {QStringLiteral("Чародей"), {QStringLiteral("чарод")}},
        {QStringLiteral("Волшебник"), {QStringLiteral("волшебн")}}
    };

    for (const ClassPattern &pattern : patterns) {
        for (const QString &fragment : pattern.fragments) {
            if (lowered.contains(fragment)) {
                return pattern.canonicalName;
            }
        }
    }

    return QString();
}

QMap<QString, QString> normalizedRaceTraits(const QMap<QString, QString> &traits)
{
    QMap<QString, QString> normalized;
    for (auto it = traits.begin(); it != traits.end(); ++it) {
        const QString title = cleanedRaceTraitTitle(it.key());
        const QString description = cleanedRaceTraitDescription(it.value());
        if (title.isEmpty() || description.isEmpty()) {
            continue;
        }

        normalized.insert(title, description);
    }
    return normalized;
}

QStringList splitDelimitedValues(const QString &text)
{
    QString cleaned = text;
    cleaned.remove(QRegularExpression(QStringLiteral("[.;].*$")));
    cleaned.remove(QRegularExpression(QStringLiteral("\\([^\\)]*\\)")));

    QStringList values = cleaned.split(',', Qt::SkipEmptyParts);
    for (QString &value : values) {
        value = value.simplified();
        value.remove(QRegularExpression(QStringLiteral("^(?:Рё|РёР»Рё)\\s+"), QRegularExpression::CaseInsensitiveOption));
        value.remove(QRegularExpression(QStringLiteral("\\s+(?:Рё|РёР»Рё)$"), QRegularExpression::CaseInsensitiveOption));
    }

    return uniqueStrings(values);
}

QStringList extractSkillProficienciesFromTrait(const QString &description)
{
    const QString normalizedDescription = normalizedName(description);
    if (!normalizedDescription.contains(QStringLiteral("влад")) &&
        !normalizedDescription.contains(QStringLiteral("навык"))) {
        return {};
    }

    QStringList matches;
    for (const QString &skill : knownSkillNames()) {
        if (normalizedDescription.contains(normalizedName(skill))) {
            matches << skill;
        }
    }
    return uniqueStrings(matches);
}

QStringList extractCategoryProficienciesFromTrait(const QString &description, const QString &categoryToken)
{
    const QString normalizedDescription = normalizedName(description);
    if (!normalizedDescription.contains(QStringLiteral("влад")) ||
        !normalizedDescription.contains(normalizedName(categoryToken))) {
        return {};
    }

    QString cleaned = description;
    cleaned.remove(QRegularExpression(QStringLiteral("^.*?(?:получаете|получаешь|владеете|владеешь)\\s+"),
                                      QRegularExpression::CaseInsensitiveOption));
    cleaned.remove(QRegularExpression(QStringLiteral("^%1(?:ами|ами и щитами|ами, щитами|ами|ой|ом|ах)?\\s+")
                                          .arg(QRegularExpression::escape(categoryToken)),
                                      QRegularExpression::CaseInsensitiveOption));
    cleaned.remove(QRegularExpression(QStringLiteral("^владение\\s+"), QRegularExpression::CaseInsensitiveOption));

    return splitDelimitedValues(cleaned);
}

QStringList collectAvailableLanguages()
{
    QStringList languages;
    for (const LanguagePattern &pattern : knownLanguagePatterns()) {
        languages << pattern.canonical;
    }

    const QList<Race> races = DatabaseManager::instance().getAllRaces();
    for (const Race &race : races) {
        for (const QString &entry : race.languages) {
            languages << extractLanguageNames(entry);
        }
    }

    const QList<Background> backgrounds = DatabaseManager::instance().getAllBackgrounds();
    for (const Background &background : backgrounds) {
        for (const QString &entry : background.languages) {
            languages << extractLanguageNames(entry);
        }
    }

    languages = uniqueStrings(languages);
    std::sort(languages.begin(), languages.end(), [](const QString &left, const QString &right) {
        return left.localeAwareCompare(right) < 0;
    });
    return languages;
}

QStringList raceAbilityIncreaseTexts(const Race &race)
{
    QStringList texts;
    for (auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        if (it.key().contains(QStringLiteral("Увеличение характеристик"), Qt::CaseInsensitive)) {
            const QString text = it.value().simplified();
            if (!text.isEmpty() && !texts.contains(text)) {
                texts << text;
            }
        }
    }
    return texts;
}

QStringList inlineAsiOptions(const QString &text)
{
    QStringList options;
    const QRegularExpression optionRegex(
        QStringLiteral("\\(([Р°-СЏa-z])\\)\\s*(.*?)(?=(?:\\([Р°-СЏa-z]\\)\\s*)|$)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator iterator = optionRegex.globalMatch(text);
    while (iterator.hasNext()) {
        const QString value = iterator.next().captured(2).simplified().trimmed();
        if (!value.isEmpty() && !options.contains(value)) {
            options << value;
        }
    }

    if (!options.isEmpty()) {
        return options;
    }

    QString normalized = text.simplified();
    if (normalized.startsWith(QStringLiteral("Либо "), Qt::CaseInsensitive)) {
        normalized.remove(0, 5);
        const QStringList parts = normalized.split(
            QRegularExpression(QStringLiteral("\\s*,?\\s*либо\\s+"), QRegularExpression::CaseInsensitiveOption),
            Qt::SkipEmptyParts);
        for (QString part : parts) {
            part = part.trimmed();
            if (!part.isEmpty() && !options.contains(part)) {
                options << part;
            }
        }
    }

    return options;
}

QMap<QString, int> fixedRaceBonusesFromText(const QString &text)
{
    struct AbilityPattern {
        QString ability;
        QString pattern;
    };

    const QList<AbilityPattern> patterns = {
        {QStringLiteral("Сила"), QStringLiteral("Сил(?:ы|а)")},
        {QStringLiteral("Ловкость"), QStringLiteral("Ловкост(?:и|ь)")},
        {QStringLiteral("Телосложение"), QStringLiteral("Телосложени(?:я|е)")},
        {QStringLiteral("Интеллект"), QStringLiteral("Интеллект(?:а)?")},
        {QStringLiteral("Мудрость"), QStringLiteral("Мудрост(?:и|ь)")},
        {QStringLiteral("Харизма"), QStringLiteral("Харизм(?:ы|а)")}
    };

    QMap<QString, int> bonuses;
    for (const AbilityPattern &pattern : patterns) {
        const QRegularExpression regex(
            QStringLiteral("%1[^.\\n]{0,60}?РЅР°\\s*(\\d+)").arg(pattern.pattern),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch match = regex.match(text);
        if (match.hasMatch()) {
            bonuses[pattern.ability] = qMax(bonuses.value(pattern.ability, 0), match.captured(1).toInt());
        }
    }
    return bonuses;
}

bool chooseAbilityIncreaseTargets(
    QWidget *parent,
    const QString &title,
    const QString &prompt,
    int choiceCount,
    int amount,
    const QStringList &excludedAbilities,
    QMap<QString, int> *bonuses)
{
    if (!bonuses || choiceCount <= 0 || amount <= 0) {
        return true;
    }

    QStringList taken = uniqueStrings(excludedAbilities);
    for (int index = 0; index < choiceCount; ++index) {
        QStringList available = allAbilityNames();
        available.erase(std::remove_if(available.begin(), available.end(), [&](const QString &ability) {
            return containsByName(taken, ability);
        }), available.end());

        if (available.isEmpty()) {
            QMessageBox::warning(parent, title, QStringLiteral("Не осталось доступных характеристик для выбора."));
            return false;
        }

        bool ok = false;
        const QString selectedAbility = QInputDialog::getItem(
            parent,
            title,
            QString("%1\nВыбор %2 из %3:").arg(prompt).arg(index + 1).arg(choiceCount),
            available,
            0,
            false,
            &ok);

        if (!ok || selectedAbility.trimmed().isEmpty()) {
            return false;
        }

        taken << selectedAbility;
        (*bonuses)[selectedAbility] += amount;
    }

    return true;
}

bool listContainsFragment(const QStringList &values, const QString &fragment)
{
    for (const QString &value : values) {
        if (value.contains(fragment, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

int characterAbilityScore(const Character *character, const QString &abilityName)
{
    if (!character) {
        return 0;
    }

    if (abilityName == "Сила") {
        return character->strength;
    }
    if (abilityName == "Ловкость") {
        return character->dexterity;
    }
    if (abilityName == "Телосложение") {
        return character->constitution;
    }
    if (abilityName == "Интеллект") {
        return character->intelligence;
    }
    if (abilityName == "Мудрость") {
        return character->wisdom;
    }
    if (abilityName == "Харизма") {
        return character->charisma;
    }

    return 0;
}

int totalFeatSlots(const QMap<QString, int> &classLevels)
{
    const CharacterProgressionRules &rules = CharacterProgressionRules::instance();
    int total = 0;
    for (auto it = classLevels.begin(); it != classLevels.end(); ++it) {
        total += rules.featSlotsForClass(it.key(), it.value());
    }
    return total;
}

int subclassChoiceLevel(const Class &cls)
{
    if (cls.subclasses.isEmpty()) {
        return 0;
    }

    static const QStringList choiceKeywords = {
        QStringLiteral("божественный домен"),
        QStringLiteral("коллегия"),
        QStringLiteral("круг друидов"),
        QStringLiteral("монастырская традиция"),
        QStringLiteral("священная клятва"),
        QStringLiteral("потусторонний покровитель"),
        QStringLiteral("чародейское происхождение"),
        QStringLiteral("первозданный путь"),
        QStringLiteral("воинский архетип"),
        QStringLiteral("плутовской архетип"),
        QStringLiteral("архетип следопыта"),
        QStringLiteral("специализация изобретателя"),
        QStringLiteral("архетип"),
        QStringLiteral("домен"),
        QStringLiteral("коллег"),
        QStringLiteral("круг"),
        QStringLiteral("путь"),
        QStringLiteral("традиц"),
        QStringLiteral("клятв"),
        QStringLiteral("покровител"),
        QStringLiteral("происхожд"),
        QStringLiteral("монастыр"),
        QStringLiteral("конклав"),
        QStringLiteral("специализац"),
        QStringLiteral("общин")
    };

    for (const ClassLevelProgression &entry : cls.progression) {
        for (const QString &feature : entry.features) {
            const QString lowered = normalizedName(feature);
            for (const QString &keyword : choiceKeywords) {
                if (lowered.contains(keyword)) {
                    return entry.level;
                }
            }
        }
    }

    int minimumLevel = 0;
    for (const ClassSubclass &subclass : cls.subclasses) {
        for (const ClassSection &section : subclass.sections) {
            if (section.levelRequirement <= 0) {
                continue;
            }
            minimumLevel = minimumLevel == 0 ? section.levelRequirement : qMin(minimumLevel, section.levelRequirement);
        }
    }

    return minimumLevel > 0 ? minimumLevel : 1;
}

QString classSectionDetailsText(const ClassSection &section)
{
    QStringList lines;
    if (!section.levelText.trimmed().isEmpty()) {
        lines << section.levelText.trimmed();
    }
    if (section.optional) {
        lines << QStringLiteral("Опциональное умение");
    }
    if (!section.description.trimmed().isEmpty()) {
        lines << section.description.trimmed();
    }
    return lines.join(QStringLiteral("\n\n"));
}

QString classSubclassDetailsText(const ClassSubclass &subclass)
{
    QStringList lines;
    if (!subclass.description.trimmed().isEmpty()) {
        lines << subclass.description.trimmed();
    }
    if (!subclass.sections.isEmpty()) {
        lines << QStringLiteral("Умения подкласса:");
        for (const ClassSection &section : subclass.sections) {
            QStringList sectionLines;
            sectionLines << section.title.trimmed();
            const QString details = classSectionDetailsText(section);
            if (!details.trimmed().isEmpty()) {
                sectionLines << details;
            }
            lines << sectionLines.join(QStringLiteral("\n"));
        }
    }
    return lines.join(QStringLiteral("\n\n"));
}

bool subclassNameExists(const Class &cls, const QString &subclassName)
{
    for (const ClassSubclass &subclass : cls.subclasses) {
        if (normalizedName(subclass.name) == normalizedName(subclassName)) {
            return true;
        }
    }
    return false;
}

ClassSubclass selectedSubclass(const Class &cls)
{
    const QString selectedName = cls.selectedSubclassName.trimmed();
    if (selectedName.isEmpty()) {
        return {};
    }

    for (const ClassSubclass &subclass : cls.subclasses) {
        if (normalizedName(subclass.name) == normalizedName(selectedName)) {
            return subclass;
        }
    }
    return {};
}

QStringList classSectionArmorProficiencies(const ClassSection &section)
{
    const QString text = section.description;
    const QString lowered = normalizedName(text);
    QStringList values;
    if (lowered.contains(QStringLiteral("тяжел")) || lowered.contains(QStringLiteral("тяжёл"))) {
        values << QStringLiteral("Тяжёлые доспехи");
    }
    if (lowered.contains(QStringLiteral("средн"))) {
        values << QStringLiteral("Средние доспехи");
    }
    if (lowered.contains(QStringLiteral("легк")) || lowered.contains(QStringLiteral("лёгк"))) {
        values << QStringLiteral("Лёгкие доспехи");
    }
    if (lowered.contains(QStringLiteral("С‰РёС‚"))) {
        values << QStringLiteral("Щиты");
    }
    return uniqueStrings(values);
}

QStringList classSectionWeaponProficiencies(const ClassSection &section)
{
    const QString text = section.description;
    const QString lowered = normalizedName(text);
    QStringList values;
    if (lowered.contains(QStringLiteral("воинск")) && lowered.contains(QStringLiteral("оруж"))) {
        values << QStringLiteral("Воинское оружие");
    }
    if (lowered.contains(QStringLiteral("прост")) && lowered.contains(QStringLiteral("оруж"))) {
        values << QStringLiteral("Простое оружие");
    }
    return uniqueStrings(values);
}

QStringList subclassArmorProficiencies(const Class &cls, int classLevel)
{
    QStringList values;
    const ClassSubclass subclass = selectedSubclass(cls);
    for (const ClassSection &section : subclass.sections) {
        if (section.levelRequirement > 0 && section.levelRequirement > classLevel) {
            continue;
        }
        values << classSectionArmorProficiencies(section);
    }
    return uniqueStrings(values);
}

QStringList subclassWeaponProficiencies(const Class &cls, int classLevel)
{
    QStringList values;
    const ClassSubclass subclass = selectedSubclass(cls);
    for (const ClassSection &section : subclass.sections) {
        if (section.levelRequirement > 0 && section.levelRequirement > classLevel) {
            continue;
        }
        values << classSectionWeaponProficiencies(section);
    }
    return uniqueStrings(values);
}

QStringList subclassToolProficiencies(const Class &cls, int classLevel)
{
    QStringList values;
    const ClassSubclass subclass = selectedSubclass(cls);
    for (const ClassSection &section : subclass.sections) {
        if (section.levelRequirement > 0 && section.levelRequirement > classLevel) {
            continue;
        }
        values << extractCategoryProficienciesFromTrait(section.description, QStringLiteral("инструмент"));
    }
    return uniqueStrings(values);
}

QStringList subclassSkillProficiencies(const Class &cls, int classLevel)
{
    QStringList values;
    const ClassSubclass subclass = selectedSubclass(cls);
    for (const ClassSection &section : subclass.sections) {
        if (section.levelRequirement > 0 && section.levelRequirement > classLevel) {
            continue;
        }
        values << extractSkillProficienciesFromTrait(section.description);
    }
    return uniqueStrings(values);
}

QStringList subclassSpellNames(const Class &cls, int classLevel)
{
    QStringList names;
    const ClassSubclass subclass = selectedSubclass(cls);
    const QRegularExpression spellRegex(QStringLiteral("\\[([^\\]]+)\\]"));

    const auto collectFromText = [&](const QString &text) {
        QRegularExpressionMatchIterator iterator = spellRegex.globalMatch(text);
        while (iterator.hasNext()) {
            names << iterator.next().captured(1).simplified();
        }
    };

    collectFromText(subclass.description);
    for (const ClassSection &section : subclass.sections) {
        if (section.levelRequirement > 0 && section.levelRequirement > classLevel) {
            continue;
        }
        collectFromText(section.description);
    }

    return uniqueStrings(names);
}

int spentAdvancementSlots(const Character *character)
{
    if (!character) {
        return 0;
    }

    return character->featNames.size() + character->abilityScoreImprovementLog.size();
}

QString advancementChoiceLabel(int slotIndex)
{
    return QStringLiteral("Повышение характеристик / черта %1").arg(slotIndex);
}

QString formatAbilityIncreaseSummary(const QMap<QString, int> &bonuses)
{
    QStringList parts;
    for (auto it = bonuses.begin(); it != bonuses.end(); ++it) {
        if (it.value() > 0) {
            parts << QStringLiteral("%1 +%2").arg(it.key()).arg(it.value());
        }
    }
    return parts.join(QStringLiteral(", "));
}

QStringList racialArmorProficiencies(const QMap<QString, QString> &traits, int level)
{
    QStringList proficiencies;
    for (auto it = traits.begin(); it != traits.end(); ++it) {
        if (!raceTraitAvailableAtLevel(it.key(), it.value(), level)) {
            continue;
        }
        proficiencies << extractCategoryProficienciesFromTrait(it.value(), QStringLiteral("доспех"));
        proficiencies << extractCategoryProficienciesFromTrait(it.value(), QStringLiteral("С‰РёС‚"));
    }
    return uniqueStrings(proficiencies);
}

QStringList racialWeaponProficiencies(const QMap<QString, QString> &traits, int level)
{
    QStringList proficiencies;
    for (auto it = traits.begin(); it != traits.end(); ++it) {
        if (!raceTraitAvailableAtLevel(it.key(), it.value(), level)) {
            continue;
        }

        const QString normalizedTitle = normalizedName(it.key());
        const QString normalizedDescription = normalizedName(it.value());
        if (normalizedTitle.contains(QStringLiteral("владение")) ||
            normalizedDescription.contains(QStringLiteral("оруж")) ||
            normalizedDescription.contains(QStringLiteral("арбал")) ||
            normalizedDescription.contains(QStringLiteral("Р»СѓРє")) ||
            normalizedDescription.contains(QStringLiteral("копь")) ||
            normalizedDescription.contains(QStringLiteral("трезуб")) ||
            normalizedDescription.contains(QStringLiteral("сеть")) ||
            normalizedDescription.contains(QStringLiteral("РјРµС‡")) ||
            normalizedDescription.contains(QStringLiteral("рапир"))) {
            const QStringList extracted = extractCategoryProficienciesFromTrait(it.value(), QStringLiteral("оруж"));
            if (!extracted.isEmpty()) {
                proficiencies << extracted;
            } else {
                proficiencies << splitDelimitedValues(it.value());
            }
        }
    }
    return uniqueStrings(proficiencies);
}

QStringList racialToolProficiencies(const QMap<QString, QString> &traits, int level)
{
    QStringList proficiencies;
    for (auto it = traits.begin(); it != traits.end(); ++it) {
        if (!raceTraitAvailableAtLevel(it.key(), it.value(), level)) {
            continue;
        }
        proficiencies << extractCategoryProficienciesFromTrait(it.value(), QStringLiteral("инструмент"));
    }
    return uniqueStrings(proficiencies);
}

QStringList racialSkillProficiencies(const QMap<QString, QString> &traits, int level)
{
    QStringList proficiencies;
    for (auto it = traits.begin(); it != traits.end(); ++it) {
        if (!raceTraitAvailableAtLevel(it.key(), it.value(), level)) {
            continue;
        }
        proficiencies << extractSkillProficienciesFromTrait(it.value());
    }
    return uniqueStrings(proficiencies);
}

bool characterCanUseSpellcasting(const Character *character, const QMap<QString, int> &classLevels)
{
    return CharacterProgressionRules::instance().characterCanUseSpellcasting(character, classLevels);
}

QStringList quotedPrerequisiteNames(const QString &prerequisite, const QString &keywordPattern)
{
    QStringList names;
    const QRegularExpression regex(
        QString("%1\\s*[В«\"]\\s*([^В»\"]+?)\\s*[В»\"]").arg(keywordPattern),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator iterator = regex.globalMatch(prerequisite);
    while (iterator.hasNext()) {
        names << iterator.next().captured(1).simplified();
    }
    return uniqueStrings(names);
}

bool raceRequirementSatisfied(const QString &prerequisite, const Character *character)
{
    if (!character) {
        return false;
    }

    const QString raceName = normalizedName(character->race());
    const QString sizeName = normalizedName(character->size);
    bool raceRequirementMentioned = false;
    bool raceRequirementMatched = false;

    const auto checkRace = [&](const QString &token, const std::function<bool()> &predicate) {
        if (prerequisite.contains(token, Qt::CaseInsensitive)) {
            raceRequirementMentioned = true;
            if (predicate()) {
                raceRequirementMatched = true;
            }
        }
    };

    checkRace(QStringLiteral("Эльф (дроу)"), [&]() { return raceName.contains(QStringLiteral("дроу")); });
    checkRace(QStringLiteral("Эльф (лесной)"), [&]() { return raceName.contains(QStringLiteral("эльф")) && raceName.contains(QStringLiteral("лес")); });
    checkRace(QStringLiteral("Гном (глубинный гном)"), [&]() { return raceName.contains(QStringLiteral("гном")) && raceName.contains(QStringLiteral("глубин")); });
    checkRace(QStringLiteral("Полуэльф"), [&]() { return raceName.contains(QStringLiteral("полуэльф")); });
    checkRace(QStringLiteral("Полуорк"), [&]() { return raceName.contains(QStringLiteral("полуорк")); });
    checkRace(QStringLiteral("Человек"), [&]() { return raceName.contains(QStringLiteral("человек")); });
    checkRace(QStringLiteral("Полурослик"), [&]() { return raceName.contains(QStringLiteral("полурос")); });
    checkRace(QStringLiteral("Драконорожд"), [&]() { return raceName.contains(QStringLiteral("драконорожд")); });
    checkRace(QStringLiteral("Тифлинг"), [&]() { return raceName.contains(QStringLiteral("тифлинг")); });
    checkRace(QStringLiteral("Дварф"), [&]() { return raceName.contains(QStringLiteral("дварф")); });
    checkRace(QStringLiteral("Гном"), [&]() { return raceName.contains(QStringLiteral("гном")); });
    checkRace(QStringLiteral("Эльф"), [&]() { return raceName.contains(QStringLiteral("эльф")) && !raceName.contains(QStringLiteral("полуэльф")); });
    checkRace(QStringLiteral("Маленькая раса"), [&]() { return sizeName.contains(QStringLiteral("мал")); });

    return !raceRequirementMentioned || raceRequirementMatched;
}

bool featSatisfiesPrerequisite(
    const Feat &feat,
    const Character *character,
    const QMap<QString, int> &classLevels,
    const QStringList &plannedFeatNames,
    bool ignoreFeatDependencies)
{
    if (!character) {
        return false;
    }

    const QString prerequisite = feat.prerequisite.simplified();
    if (prerequisite.isEmpty()) {
        return true;
    }

    const QRegularExpression levelRegex(QStringLiteral("(\\d+)\\s*уров"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch levelMatch = levelRegex.match(prerequisite);
    if (levelMatch.hasMatch() && character->level < levelMatch.captured(1).toInt()) {
        return false;
    }

    const QRegularExpression abilityRegex(
        QStringLiteral("(Сила|Ловкость|Телосложение|Интеллект|Мудрость|Харизма)(?:\\s+или\\s+(Сила|Ловкость|Телосложение|Интеллект|Мудрость|Харизма))?\\s+(\\d+)\\s+или\\s+выше"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch abilityMatch = abilityRegex.match(prerequisite);
    if (abilityMatch.hasMatch()) {
        const int minimumScore = abilityMatch.captured(3).toInt();
        QStringList abilityOptions = {abilityMatch.captured(1).simplified()};
        if (!abilityMatch.captured(2).trimmed().isEmpty()) {
            abilityOptions << abilityMatch.captured(2).simplified();
        }

        bool abilitySatisfied = false;
        for (const QString &ability : abilityOptions) {
            if (characterAbilityScore(character, ability) >= minimumScore) {
                abilitySatisfied = true;
                break;
            }
        }

        if (!abilitySatisfied) {
            return false;
        }
    }

    if (prerequisite.contains(QStringLiteral("Способность накладывать хотя бы одно заклинание"), Qt::CaseInsensitive) ||
        prerequisite.contains(QStringLiteral("Использование заклинаний"), Qt::CaseInsensitive) ||
        prerequisite.contains(QStringLiteral("Магия договора"), Qt::CaseInsensitive)) {
        if (!characterCanUseSpellcasting(character, classLevels)) {
            return false;
        }
    }

    if (prerequisite.contains(QStringLiteral("Владение лёгкими доспехами"), Qt::CaseInsensitive) &&
        !listContainsFragment(character->armorProficiencies, QStringLiteral("Р»С‘Рі")) &&
        !listContainsFragment(character->armorProficiencies, QStringLiteral("легк"))) {
        return false;
    }

    if (prerequisite.contains(QStringLiteral("Владение средними доспехами"), Qt::CaseInsensitive) &&
        !listContainsFragment(character->armorProficiencies, QStringLiteral("средн"))) {
        return false;
    }

    if (prerequisite.contains(QStringLiteral("Владение тяжёлыми доспехами"), Qt::CaseInsensitive) &&
        !listContainsFragment(character->armorProficiencies, QStringLiteral("С‚СЏР¶"))) {
        return false;
    }

    if ((prerequisite.contains(QStringLiteral("воинским оружием"), Qt::CaseInsensitive) ||
         prerequisite.contains(QStringLiteral("воинское оружие"), Qt::CaseInsensitive)) &&
        !listContainsFragment(character->weaponProficiencies, QStringLiteral("воин"))) {
        return false;
    }

    const QStringList requiredBackgrounds = quotedPrerequisiteNames(prerequisite, QStringLiteral("предыстори(?:я|и)"));
    if (!requiredBackgrounds.isEmpty() && !containsByName(requiredBackgrounds, character->background)) {
        return false;
    }

    if (!raceRequirementSatisfied(prerequisite, character)) {
        return false;
    }

    if (!ignoreFeatDependencies) {
        const QStringList requiredFeats = quotedPrerequisiteNames(prerequisite, QStringLiteral("черт[аы]"));
        if (!requiredFeats.isEmpty()) {
            bool featDependencySatisfied = false;
            for (const QString &requiredFeat : requiredFeats) {
                if (containsByName(plannedFeatNames, requiredFeat)) {
                    featDependencySatisfied = true;
                    break;
                }
            }
            if (!featDependencySatisfied) {
                return false;
            }
        }
    }

    if (prerequisite.contains(QStringLiteral("Отсутствие другой Метки Дракона"), Qt::CaseInsensitive)) {
        for (const QString &featName : plannedFeatNames) {
            if (featName.contains(QStringLiteral("Метка Дракона"), Qt::CaseInsensitive) &&
                !featName.contains(QStringLiteral("Отсутствие другой Метки Дракона"), Qt::CaseInsensitive)) {
                return false;
            }
        }
    }

    return true;
}

QString backgroundDetailsText(const Background &background)
{
    QStringList lines;
    if (!background.source.isEmpty()) {
        lines << QString("Источник: %1").arg(background.source);
    }
    if (!background.description.isEmpty()) {
        lines << QString("\n%1").arg(background.description);
    }
    if (!background.skillProficiencies.isEmpty()) {
        lines << QString("\nНавыки: %1").arg(background.skillProficiencies.join(", "));
    }
    if (!background.toolProficiencies.isEmpty()) {
        lines << QString("Инструменты: %1").arg(background.toolProficiencies.join(", "));
    }
    if (!background.languages.isEmpty()) {
        lines << QString("Языки: %1").arg(background.languages.join(", "));
    }
    if (!background.equipment.isEmpty()) {
        lines << QString("Снаряжение: %1").arg(background.equipment.join(", "));
    }
    if (!background.featureName.isEmpty()) {
        lines << QString("\nУмение: %1").arg(background.featureName);
    }
    if (!background.featureDescription.isEmpty()) {
        lines << background.featureDescription;
    }
    return lines.join("\n");
}

QString featDetailsText(const Feat &feat)
{
    QStringList lines;
    if (!feat.source.isEmpty()) {
        lines << QString("Источник: %1").arg(feat.source);
    }
    if (!feat.prerequisite.isEmpty()) {
        lines << QString("Требование: %1").arg(feat.prerequisite);
    }
    if (!feat.description.isEmpty()) {
        lines << QString("\n%1").arg(feat.description);
    }
    if (!feat.benefits.isEmpty()) {
        lines << "\nЭффекты:";
        for (const QString &benefit : feat.benefits) {
            lines << QString("• %1").arg(benefit);
        }
    }
    return lines.join("\n");
}

QString itemDetailsText(const Item &item)
{
    QStringList lines;
    lines << QString("Тип: %1").arg(item.type.isEmpty() ? "-" : item.type);
    lines << QString("Редкость: %1").arg(item.rarity.isEmpty() ? "-" : item.rarity);
    lines << QString("Цена: %1").arg(item.cost.isEmpty() ? "-" : item.cost);
    lines << QString("Вес: %1").arg(item.weight.isEmpty() ? "-" : item.weight);
    if (!item.description.isEmpty()) {
        lines << QString("\n%1").arg(item.description);
    }
    return lines.join("\n");
}

QString spellDetailsText(const Spell &spell)
{
    QStringList lines;
    lines << QString("Уровень: %1").arg(spell.level == 0 ? QStringLiteral("заговор") : QString::number(spell.level));
    lines << QString("Школа: %1").arg(spell.school.isEmpty() ? "-" : spell.school);
    lines << QString("Накладывание: %1").arg(spell.castingTime.isEmpty() ? "-" : spell.castingTime);
    lines << QString("Дистанция: %1").arg(spell.range.isEmpty() ? "-" : spell.range);
    lines << QString("Компоненты: %1").arg(spell.components.isEmpty() ? "-" : spell.components);
    lines << QString("Длительность: %1").arg(spell.duration.isEmpty() ? "-" : spell.duration);
    lines << QString("Классы: %1").arg(spell.classes.isEmpty() ? "-" : spell.classes);
    if (!spell.description.isEmpty()) {
        lines << QString("\n%1").arg(spell.description);
    }
    return lines.join("\n");
}

struct PdfNamedAbilityEntry {
    QString name;
    QString ability;
};

QString pdfDashIfEmpty(QString value)
{
    value = value.simplified();
    return value.isEmpty() ? QStringLiteral("-") : value;
}

QString pdfDashIfBlank(QString value)
{
    value = value.trimmed();
    return value.isEmpty() ? QStringLiteral("-") : value;
}

QString pdfSignedValue(int value)
{
    return QStringLiteral("%1%2")
        .arg(value >= 0 ? QStringLiteral("+") : QString())
        .arg(value);
}

QStringList pdfNormalizedList(const QStringList &values)
{
    QStringList normalized;
    for (const QString &value : values) {
        normalized << normalizedName(value);
    }
    return normalized;
}

const QList<PdfNamedAbilityEntry> &pdfSavingThrowDefinitions()
{
    static const QList<PdfNamedAbilityEntry> definitions = {
        {QStringLiteral("Сила"), QStringLiteral("Сила")},
        {QStringLiteral("Ловкость"), QStringLiteral("Ловкость")},
        {QStringLiteral("Телосложение"), QStringLiteral("Телосложение")},
        {QStringLiteral("Интеллект"), QStringLiteral("Интеллект")},
        {QStringLiteral("Мудрость"), QStringLiteral("Мудрость")},
        {QStringLiteral("Харизма"), QStringLiteral("Харизма")}
    };
    return definitions;
}

const QList<PdfNamedAbilityEntry> &pdfSkillDefinitions()
{
    static const QList<PdfNamedAbilityEntry> definitions = {
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

QString pdfClassSummary(const Character *character)
{
    if (!character) {
        return QStringLiteral("-");
    }

    const QString directSummary = character->characterClass().simplified();
    if (!directSummary.isEmpty()) {
        return directSummary;
    }

    QStringList order = character->classOrder;
    if (order.isEmpty()) {
        order = character->classLevels.keys();
    }

    QStringList parts;
    for (const QString &className : order) {
        const int classLevel = character->classLevels.value(className, 0);
        if (classLevel <= 0) {
            continue;
        }
        const QString subclass = character->subclassSelections.value(className).simplified();
        parts << (subclass.isEmpty()
            ? QStringLiteral("%1 %2").arg(className).arg(classLevel)
            : QStringLiteral("%1 %2 (%3)").arg(className).arg(classLevel).arg(subclass));
    }

    return parts.isEmpty() ? QStringLiteral("%1 ур.").arg(character->level) : parts.join(QStringLiteral(" / "));
}

QString pdfSpeedSummary(const Character *character)
{
    if (!character) {
        return QStringLiteral("-");
    }

    QStringList parts;
    parts << QStringLiteral("%1 фт.").arg(character->speed);
    if (character->flyingSpeed > 0) {
        parts << QStringLiteral("полет %1 фт.").arg(character->flyingSpeed);
    }
    return parts.join(QStringLiteral(", "));
}

QStringList pdfFeatureLines(const Character *character)
{
    QStringList lines;
    if (!character) {
        return lines;
    }

    if (!character->backgroundFeatureName.simplified().isEmpty()) {
        lines << QStringLiteral("Предыстория: %1").arg(character->backgroundFeatureName.simplified());
        if (!character->backgroundFeatureDescription.simplified().isEmpty()) {
            lines << character->backgroundFeatureDescription.simplified();
        }
    }

    for (auto it = character->traits.begin(); it != character->traits.end(); ++it) {
        const QString title = it.key().simplified();
        const QString description = it.value().simplified();
        if (title.isEmpty() && description.isEmpty()) {
            continue;
        }
        lines << (description.isEmpty() ? title : QStringLiteral("%1: %2").arg(title, description));
    }

    for (const QString &featName : character->featNames) {
        const QString description = character->featDescriptions.value(featName).simplified();
        lines << (description.isEmpty()
            ? QStringLiteral("Черта: %1").arg(featName)
            : QStringLiteral("Черта: %1 - %2").arg(featName, description));
    }

    for (auto it = character->classFeatureChoices.begin(); it != character->classFeatureChoices.end(); ++it) {
        lines << QStringLiteral("Выбор умения: %1 - %2").arg(it.key(), it.value());
    }

    if (!character->abilityScoreImprovementLog.isEmpty()) {
        lines << QStringLiteral("Улучшения характеристик: %1").arg(character->abilityScoreImprovementLog.join(QStringLiteral("; ")));
    }

    return lines;
}

QStringList pdfAppearanceLines(const Character *character)
{
    QStringList lines;
    if (!character) {
        return lines;
    }

    lines << QStringLiteral("Возраст: %1").arg(pdfDashIfEmpty(character->age));
    lines << QStringLiteral("Рост: %1").arg(pdfDashIfEmpty(character->height));
    lines << QStringLiteral("Вес: %1").arg(pdfDashIfEmpty(character->weight));
    lines << QStringLiteral("Кожа: %1").arg(pdfDashIfEmpty(character->skin));
    lines << QStringLiteral("Волосы: %1").arg(pdfDashIfEmpty(character->hair));

    if (!character->appearance.trimmed().isEmpty()) {
        lines << QString();
        lines << QStringLiteral("Описание внешности:");
        lines << character->appearance.trimmed();
    }

    if (!character->personalHistory.trimmed().isEmpty()) {
        lines << QString();
        lines << QStringLiteral("Личная история:");
        lines << character->personalHistory.trimmed();
    }

    return lines;
}

QStringList pdfProficiencyLines(const Character *character)
{
    QStringList lines;
    if (!character) {
        return lines;
    }

    lines << QStringLiteral("Языки: %1").arg(character->languages.isEmpty() ? QStringLiteral("-") : character->languages.join(QStringLiteral(", ")));
    lines << QStringLiteral("Навыки: %1").arg(character->skillProficiencies.isEmpty() ? QStringLiteral("-") : character->skillProficiencies.join(QStringLiteral(", ")));
    lines << QStringLiteral("Инструменты: %1").arg(character->toolProficiencies.isEmpty() ? QStringLiteral("-") : character->toolProficiencies.join(QStringLiteral(", ")));
    lines << QStringLiteral("Спасброски: %1").arg(character->savingThrowProficiencies.isEmpty() ? QStringLiteral("-") : character->savingThrowProficiencies.join(QStringLiteral(", ")));
    lines << QStringLiteral("Доспехи: %1").arg(character->armorProficiencies.isEmpty() ? QStringLiteral("-") : character->armorProficiencies.join(QStringLiteral(", ")));
    lines << QStringLiteral("Оружие: %1").arg(character->weaponProficiencies.isEmpty() ? QStringLiteral("-") : character->weaponProficiencies.join(QStringLiteral(", ")));
    return lines;
}

QStringList pdfInventoryLines(const Character *character)
{
    QStringList lines;
    if (!character) {
        return lines;
    }

    for (const Item &item : character->inventory) {
        QString line = pdfDashIfEmpty(item.name);
        QStringList meta;
        if (item.quantity > 1) {
            meta << QStringLiteral("x%1").arg(item.quantity);
        }
        if (item.isEquipped) {
            meta << QStringLiteral("надето");
        }
        if (!item.type.simplified().isEmpty()) {
            meta << item.type.simplified();
        }
        if (!item.weight.simplified().isEmpty()) {
            meta << QStringLiteral("вес %1").arg(item.weight.simplified());
        }
        if (!meta.isEmpty()) {
            line += QStringLiteral(" (%1)").arg(meta.join(QStringLiteral(", ")));
        }
        if (!item.description.simplified().isEmpty()) {
            line += QStringLiteral(": %1").arg(item.description.simplified());
        }
        lines << line;
    }

    return lines;
}

QStringList pdfSpellSlotLines(const Character *character)
{
    QStringList lines;
    if (!character) {
        return lines;
    }

    for (auto it = character->spellSlotCurrent.begin(); it != character->spellSlotCurrent.end(); ++it) {
        const QStringList parts = it.key().split(QStringLiteral("|"));
        if (parts.size() == 2) {
            lines << QStringLiteral("%1, %2 ур.: %3").arg(parts.at(0)).arg(parts.at(1)).arg(it.value());
        } else {
            lines << QStringLiteral("%1: %2").arg(it.key()).arg(it.value());
        }
    }

    return lines;
}

QStringList pdfSpellcastingClassNames(const Character *character)
{
    QStringList classes;
    if (!character) {
        return classes;
    }

    QStringList order = character->classOrder;
    if (order.isEmpty()) {
        order = character->classLevels.keys();
    }

    for (const QString &className : order) {
        if (character->classLevels.value(className, 0) <= 0) {
            continue;
        }

        const QString normalizedClass = normalizedName(className);
        if (normalizedClass == normalizedName(QStringLiteral("Бард")) ||
            normalizedClass == normalizedName(QStringLiteral("Жрец")) ||
            normalizedClass == normalizedName(QStringLiteral("Друид")) ||
            normalizedClass == normalizedName(QStringLiteral("Волшебник")) ||
            normalizedClass == normalizedName(QStringLiteral("Чародей")) ||
            normalizedClass == normalizedName(QStringLiteral("Паладин")) ||
            normalizedClass == normalizedName(QStringLiteral("Следопыт")) ||
            normalizedClass == normalizedName(QStringLiteral("Изобретатель")) ||
            normalizedClass == normalizedName(QStringLiteral("Колдун"))) {
            classes << className;
        }
    }

    return classes;
}

QString pdfSpellcastingAbilityName(const QString &className)
{
    const QString normalizedClass = normalizedName(className);
    if (normalizedClass == normalizedName(QStringLiteral("Бард")) ||
        normalizedClass == normalizedName(QStringLiteral("Паладин")) ||
        normalizedClass == normalizedName(QStringLiteral("Колдун")) ||
        normalizedClass == normalizedName(QStringLiteral("Чародей"))) {
        return QStringLiteral("Харизма");
    }
    if (normalizedClass == normalizedName(QStringLiteral("Волшебник")) ||
        normalizedClass == normalizedName(QStringLiteral("Изобретатель"))) {
        return QStringLiteral("Интеллект");
    }
    if (normalizedClass == normalizedName(QStringLiteral("Жрец")) ||
        normalizedClass == normalizedName(QStringLiteral("Друид")) ||
        normalizedClass == normalizedName(QStringLiteral("Следопыт"))) {
        return QStringLiteral("Мудрость");
    }
    return QStringLiteral("Харизма");
}

QString pdfBestSpellcastingAbilityName(const Character *character)
{
    const QStringList castingClasses = pdfSpellcastingClassNames(character);
    if (!castingClasses.isEmpty()) {
        return pdfSpellcastingAbilityName(castingClasses.first());
    }

    if (!character) {
        return QStringLiteral("Харизма");
    }

    const QMap<QString, int> mentalScores = {
        {QStringLiteral("Интеллект"), character->intelligence},
        {QStringLiteral("Мудрость"), character->wisdom},
        {QStringLiteral("Харизма"), character->charisma}
    };

    QString best = QStringLiteral("Харизма");
    int bestScore = character->charisma;
    for (auto it = mentalScores.begin(); it != mentalScores.end(); ++it) {
        if (it.value() > bestScore) {
            best = it.key();
            bestScore = it.value();
        }
    }
    return best;
}

int pdfSpellcastingAbilityScore(const Character *character, const QString &abilityName)
{
    return characterAbilityScore(character, abilityName);
}

QString pdfSpellcastingStatText(const Character *character)
{
    const QString abilityName = pdfBestSpellcastingAbilityName(character);
    const int score = pdfSpellcastingAbilityScore(character, abilityName);
    const QString shortName = abilityName == QStringLiteral("Интеллект")
        ? QStringLiteral("ИНТ")
        : abilityName == QStringLiteral("Мудрость")
            ? QStringLiteral("МДР")
            : QStringLiteral("ХАР");
    return QStringLiteral("%1 %2 (%3)").arg(shortName).arg(score).arg(pdfSignedValue(Character::abilityModifier(score)));
}

QMap<int, QStringList> pdfGroupedSpellNames(const Character *character)
{
    QMap<int, QStringList> grouped;
    if (!character) {
        return grouped;
    }

    QStringList seen;
    auto appendSpell = [&](const Spell &spell) {
        const QString name = spell.name.simplified();
        if (name.isEmpty()) {
            return;
        }
        const int level = qBound(0, spell.level, 9);
        const QString key = QStringLiteral("%1|%2").arg(level).arg(normalizedName(name));
        if (seen.contains(key)) {
            return;
        }
        seen << key;
        grouped[level] << name;
    };

    for (const Spell &spell : character->spells) {
        appendSpell(spell);
    }
    for (const Spell &spell : character->spellbook) {
        appendSpell(spell);
    }

    for (auto it = grouped.begin(); it != grouped.end(); ++it) {
        it.value().sort(Qt::CaseInsensitive);
    }

    return grouped;
}

void pdfSetFont(QPainter &painter, int pointSize, bool bold = false, bool italic = false)
{
    QFont font = painter.font();
    font.setPointSize(pointSize);
    font.setBold(bold);
    font.setItalic(italic);
    painter.setFont(font);
}

void pdfDrawPanel(QPainter &painter, const QRectF &rect, const QString &title)
{
    painter.save();
    painter.setPen(QPen(QColor(QStringLiteral("#5e503f")), 1.2));
    painter.setBrush(QColor(QStringLiteral("#fffaf5")));
    painter.drawRoundedRect(rect, 9, 9);

    if (!title.isEmpty()) {
        QRectF strip(rect.left() + 12, rect.top() - 11, qMin(rect.width() - 24, 330.0), 22);
        painter.fillRect(strip, QColor(QStringLiteral("#ffffff")));
        painter.setPen(QColor(QStringLiteral("#0a0908")));
        pdfSetFont(painter, 8, true);
        painter.drawText(strip.adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, title);
    }
    painter.restore();
}

void pdfDrawField(QPainter &painter, const QRectF &rect, const QString &label, const QString &value, int valuePointSize = 10)
{
    painter.save();
    painter.setPen(QPen(QColor(QStringLiteral("#c6ac8f")), 1.0));
    painter.setBrush(QColor(QStringLiteral("#fffaf5")));
    painter.drawRoundedRect(rect, 7, 7);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#eae0d5")));
    painter.drawRect(QRectF(rect.left() + 1, rect.top() + 1, rect.width() - 2, 20));
    painter.setPen(QPen(QColor(QStringLiteral("#c6ac8f")), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect, 7, 7);

    painter.setPen(QColor(QStringLiteral("#0a0908")));
    pdfSetFont(painter, 7, false);
    painter.drawText(rect.adjusted(7, 3, -7, -rect.height() + 20), Qt::AlignLeft | Qt::AlignVCenter, label);

    pdfSetFont(painter, valuePointSize, true);
    painter.setClipRect(rect.adjusted(5, 20, -5, -4));
    painter.drawText(rect.adjusted(7, 20, -7, -4), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, pdfDashIfEmpty(value));
    painter.restore();
}

void pdfDrawAbilityBox(QPainter &painter, const QRectF &rect, const QString &label, int score)
{
    painter.save();
    painter.setPen(QPen(QColor(QStringLiteral("#5e503f")), 1.1));
    painter.setBrush(QColor(QStringLiteral("#fffaf5")));
    painter.drawRoundedRect(rect, 12, 12);

    painter.setPen(QColor(QStringLiteral("#0a0908")));
    pdfSetFont(painter, 9, true);
    painter.drawText(QRectF(rect.left() + 7, rect.top() + 8, rect.width() - 14, 20), Qt::AlignCenter, label.toUpper());

    pdfSetFont(painter, 21, true);
    painter.drawText(QRectF(rect.left() + 10, rect.top() + 35, rect.width() - 20, 42), Qt::AlignCenter, pdfSignedValue(Character::abilityModifier(score)));

    painter.setPen(QPen(QColor(QStringLiteral("#c6ac8f")), 1.0));
    painter.setBrush(QColor(QStringLiteral("#ffffff")));
    const QRectF scoreRect(rect.left() + rect.width() * 0.22, rect.bottom() - 33, rect.width() * 0.56, 27);
    painter.drawRoundedRect(scoreRect, 13, 13);
    painter.setPen(QColor(QStringLiteral("#0a0908")));
    pdfSetFont(painter, 11, true);
    painter.drawText(scoreRect, Qt::AlignCenter, QString::number(score));
    painter.restore();
}

void pdfDrawTextBlock(QPainter &painter, const QRectF &rect, const QString &title, const QString &text, int pointSize = 8)
{
    pdfDrawPanel(painter, rect, title);
    painter.save();
    painter.setPen(QColor(QStringLiteral("#0a0908")));
    pdfSetFont(painter, pointSize);
    const QRectF textRect = rect.adjusted(12, 22, -12, -12);
    painter.setClipRect(textRect);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, pdfDashIfBlank(text));
    painter.restore();
}

void pdfDrawLineList(QPainter &painter, const QRectF &rect, const QString &title, QStringList lines, int pointSize = 8)
{
    if (lines.isEmpty()) {
        lines << QStringLiteral("-");
    }

    pdfDrawPanel(painter, rect, title);
    painter.save();
    painter.setPen(QColor(QStringLiteral("#0a0908")));
    pdfSetFont(painter, pointSize);
    QFontMetrics metrics(painter.font());
    const qreal left = rect.left() + 12;
    qreal y = rect.top() + 25;
    const qreal right = rect.right() - 12;
    const qreal bottom = rect.bottom() - 10;
    const int lineHeight = metrics.lineSpacing() + 2;

    for (const QString &rawLine : lines) {
        if (y + lineHeight > bottom) {
            painter.drawText(QRectF(left, y, right - left, lineHeight), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("..."));
            break;
        }
        const QString line = metrics.elidedText(rawLine.simplified(), Qt::ElideRight, static_cast<int>(right - left));
        painter.drawText(QRectF(left, y, right - left, lineHeight), Qt::AlignLeft | Qt::AlignVCenter, line);
        y += lineHeight;
    }

    painter.restore();
}

void pdfDrawSpellLevelBlock(QPainter &painter, const QRectF &rect, int level, const QStringList &spells)
{
    painter.save();
    painter.setPen(QPen(QColor(QStringLiteral("#5e503f")), 1.1));
    painter.setBrush(QColor(QStringLiteral("#ffffff")));
    painter.drawRoundedRect(rect, 8, 8);

    const QRectF headerRect(rect.left() + 42, rect.top() + 10, rect.width() - 54, 38);
    painter.setPen(QPen(QColor(QStringLiteral("#0a0908")), 1.0));
    painter.setBrush(QColor(QStringLiteral("#dfe6ff")));
    painter.drawRoundedRect(headerRect, 8, 8);

    const QRectF levelRect(rect.left() + 9, rect.top() + 8, 32, 42);
    painter.setBrush(QColor(QStringLiteral("#fffaf5")));
    painter.drawRoundedRect(levelRect, 8, 8);
    pdfSetFont(painter, 12, true);
    painter.drawText(levelRect, Qt::AlignCenter, QString::number(level));

    pdfSetFont(painter, 8, true);
    painter.drawText(headerRect.adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
                     level == 0 ? QStringLiteral("Заговоры") : QStringLiteral("%1 уровень").arg(level));

    pdfSetFont(painter, 7);
    QFontMetrics metrics(painter.font());
    const qreal left = rect.left() + 20;
    const qreal right = rect.right() - 14;
    qreal y = rect.top() + 62;
    const qreal bottom = rect.bottom() - 14;
    const int lineHeight = metrics.lineSpacing() + 6;
    const int visibleRows = qMax(1, static_cast<int>((bottom - y) / lineHeight));
    const int rowCount = qMax(visibleRows, qMin(visibleRows, spells.size()));

    painter.setPen(QPen(QColor(QStringLiteral("#b9c7f4")), 0.8));
    for (int row = 0; row < visibleRows; ++row) {
        const QRectF lineRect(left + 18, y + row * lineHeight + 3, right - left - 18, 13);
        painter.fillRect(lineRect, QColor(QStringLiteral("#e6ebff")));
        painter.drawLine(QPointF(lineRect.left(), lineRect.bottom() + 2), QPointF(lineRect.right(), lineRect.bottom() + 2));
    }

    painter.setPen(QColor(QStringLiteral("#0a0908")));
    for (int index = 0; index < rowCount; ++index) {
        const QRectF checkRect(left, y + index * lineHeight + 5, 7, 7);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(checkRect);

        if (index >= spells.size()) {
            continue;
        }
        QString spellName = metrics.elidedText(spells.at(index), Qt::ElideRight, static_cast<int>(right - left - 24));
        painter.drawText(QRectF(left + 18, y + index * lineHeight - 1, right - left - 18, lineHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, spellName);
    }

    if (spells.size() > visibleRows) {
        painter.drawText(QRectF(left + 18, bottom - lineHeight, right - left - 18, lineHeight),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("... ещё %1").arg(spells.size() - visibleRows));
    }

    painter.restore();
}

QStringList pdfCheckLines(const Character *character, const QList<PdfNamedAbilityEntry> &entries, const QStringList &proficiencies)
{
    QStringList lines;
    if (!character) {
        return lines;
    }

    const QStringList normalizedProficiencies = pdfNormalizedList(proficiencies);
    for (const PdfNamedAbilityEntry &entry : entries) {
        const bool proficient = normalizedProficiencies.contains(normalizedName(entry.name));
        const int baseModifier = Character::abilityModifier(characterAbilityScore(character, entry.ability));
        const int totalModifier = baseModifier + (proficient ? character->proficiencyBonus : 0);
        lines << QStringLiteral("[%1] %2 %3")
                     .arg(proficient ? QStringLiteral("x") : QStringLiteral(" "))
                     .arg(entry.name)
                     .arg(pdfSignedValue(totalModifier));
    }
    return lines;
}

void pdfPreparePage(QPainter &painter)
{
    painter.setWindow(0, 0, 1190, 1684);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(QRectF(0, 0, 1190, 1684), QColor(QStringLiteral("#ffffff")));
}

void pdfDrawFooter(QPainter &painter, int pageNumber)
{
    painter.save();
    painter.setPen(QColor(QStringLiteral("#5e503f")));
    pdfSetFont(painter, 7);
    painter.drawText(QRectF(30, 1645, 1130, 22), Qt::AlignCenter, QStringLiteral("DnD Helper - лист персонажа - стр. %1").arg(pageNumber));
    painter.restore();
}

void pdfDrawPageTitle(QPainter &painter, const QString &title)
{
    painter.save();
    painter.setPen(QColor(QStringLiteral("#a5382f")));
    pdfSetFont(painter, 18, true);
    painter.drawText(QRectF(30, 18, 1130, 32), Qt::AlignLeft | Qt::AlignVCenter, title);
    painter.setPen(QPen(QColor(QStringLiteral("#5e503f")), 1.3));
    painter.drawLine(QPointF(30, 58), QPointF(1160, 58));
    painter.restore();
}

void pdfDrawCharacterHeader(QPainter &painter, const Character *character)
{
    pdfDrawPanel(painter, QRectF(30, 70, 1130, 165), QString());
    pdfDrawField(painter, QRectF(50, 92, 350, 58), QStringLiteral("Имя персонажа"), character ? character->name() : QString());
    pdfDrawField(painter, QRectF(420, 92, 420, 58), QStringLiteral("Класс и уровень"), pdfClassSummary(character), 9);
    pdfDrawField(painter, QRectF(860, 92, 280, 58), QStringLiteral("Предыстория"), character ? character->background : QString());
    pdfDrawField(painter, QRectF(50, 162, 250, 52), QStringLiteral("Раса"), character ? character->race() : QString());
    pdfDrawField(painter, QRectF(320, 162, 250, 52), QStringLiteral("Мировоззрение"), character ? character->alignment : QString());
    pdfDrawField(painter, QRectF(590, 162, 170, 52), QStringLiteral("Опыт"), character ? QString::number(character->experiencePoints) : QString());
    pdfDrawField(painter, QRectF(780, 162, 170, 52), QStringLiteral("Бонус мастерства"), character ? pdfSignedValue(character->proficiencyBonus) : QString());
    pdfDrawField(painter, QRectF(970, 162, 170, 52), QStringLiteral("Размер"), character ? character->size : QString());
}

void pdfDrawCharacterSheetPage(QPainter &painter, const Character *character)
{
    pdfPreparePage(painter);
    pdfDrawPageTitle(painter, QStringLiteral("Лист персонажа"));
    pdfDrawCharacterHeader(painter, character);

    const QList<QPair<QString, int>> abilities = {
        {QStringLiteral("Сила"), character ? character->strength : 10},
        {QStringLiteral("Ловкость"), character ? character->dexterity : 10},
        {QStringLiteral("Телосложение"), character ? character->constitution : 10},
        {QStringLiteral("Интеллект"), character ? character->intelligence : 10},
        {QStringLiteral("Мудрость"), character ? character->wisdom : 10},
        {QStringLiteral("Харизма"), character ? character->charisma : 10}
    };

    qreal abilityY = 275;
    for (const auto &ability : abilities) {
        pdfDrawAbilityBox(painter, QRectF(30, abilityY, 225, 118), ability.first, ability.second);
        abilityY += 128;
    }

    pdfDrawPanel(painter, QRectF(275, 275, 280, 310), QStringLiteral("Боевые параметры"));
    pdfDrawField(painter, QRectF(292, 305, 75, 58), QStringLiteral("КД"), character ? QString::number(character->armorClass) : QString(), 12);
    pdfDrawField(painter, QRectF(382, 305, 75, 58), QStringLiteral("Инициатива"), character ? pdfSignedValue(character->initiative) : QString(), 12);
    pdfDrawField(painter, QRectF(472, 305, 65, 58), QStringLiteral("Скорость"), pdfSpeedSummary(character), 9);
    pdfDrawField(painter, QRectF(292, 378, 245, 52), QStringLiteral("Хиты максимум"), character ? QString::number(character->maxHp) : QString(), 11);
    pdfDrawField(painter, QRectF(292, 438, 245, 52), QStringLiteral("Хиты текущие"), character ? QString::number(character->currentHp) : QString(), 11);
    pdfDrawField(painter, QRectF(292, 498, 245, 52), QStringLiteral("Временные хиты"), character ? QString::number(character->tempHp) : QString(), 11);

    pdfDrawLineList(
        painter,
        QRectF(275, 610, 280, 190),
        QStringLiteral("Спасброски"),
        pdfCheckLines(character, pdfSavingThrowDefinitions(), character ? character->savingThrowProficiencies : QStringList()),
        8);

    pdfDrawLineList(
        painter,
        QRectF(275, 825, 280, 490),
        QStringLiteral("Навыки"),
        pdfCheckLines(character, pdfSkillDefinitions(), character ? character->skillProficiencies : QStringList()),
        7);

    pdfDrawLineList(
        painter,
        QRectF(275, 1340, 280, 185),
        QStringLiteral("Владения и языки"),
        pdfProficiencyLines(character),
        7);

    pdfDrawTextBlock(
        painter,
        QRectF(580, 275, 580, 420),
        QStringLiteral("Инвентарь"),
        pdfInventoryLines(character).isEmpty()
            ? QStringLiteral("Инвентарь пуст.")
            : pdfInventoryLines(character).join(QStringLiteral("\n")),
        8);

    pdfDrawTextBlock(
        painter,
        QRectF(580, 725, 580, 560),
        QStringLiteral("Черты, умения и особенности"),
        pdfFeatureLines(character).join(QStringLiteral("\n\n")),
        8);

    QStringList attackLines = character ? character->attacks : QStringList();
    pdfDrawLineList(
        painter,
        QRectF(580, 1315, 580, 210),
        QStringLiteral("Атаки"),
        attackLines.isEmpty() ? QStringList{QStringLiteral("Атаки не заполнены.")} : attackLines,
        8);

    pdfDrawFooter(painter, 1);
}

void pdfDrawCharacterDetailsPage(QPainter &painter, const Character *character)
{
    pdfPreparePage(painter);
    pdfDrawPageTitle(painter, QStringLiteral("История, снаряжение и заметки"));

    pdfDrawField(painter, QRectF(30, 75, 360, 60), QStringLiteral("Имя персонажа"), character ? character->name() : QString());
    pdfDrawField(painter, QRectF(410, 75, 360, 60), QStringLiteral("Класс и уровень"), pdfClassSummary(character), 9);
    pdfDrawField(painter, QRectF(790, 75, 370, 60), QStringLiteral("Раса и предыстория"),
                 character ? QStringLiteral("%1, %2").arg(pdfDashIfEmpty(character->race()), pdfDashIfEmpty(character->background)) : QString(), 9);

    pdfDrawTextBlock(
        painter,
        QRectF(30, 170, 540, 515),
        QStringLiteral("История и внешность"),
        pdfAppearanceLines(character).join(QStringLiteral("\n")),
        8);

    pdfDrawTextBlock(
        painter,
        QRectF(590, 170, 570, 260),
        QStringLiteral("Атаки и боевые заметки"),
        (character && !character->attacks.isEmpty()) ? character->attacks.join(QStringLiteral("\n")) : QStringLiteral("Атаки не заполнены."),
        8);

    pdfDrawTextBlock(
        painter,
        QRectF(590, 460, 570, 560),
        QStringLiteral("Дополнительные черты"),
        pdfFeatureLines(character).join(QStringLiteral("\n\n")),
        8);

    pdfDrawTextBlock(
        painter,
        QRectF(30, 715, 540, 350),
        QStringLiteral("Владения, языки и источники"),
        pdfProficiencyLines(character).join(QStringLiteral("\n")),
        8);

    QString notes;
    if (character) {
        QStringList noteLines;
        if (!character->backgroundDescription.simplified().isEmpty()) {
            noteLines << QStringLiteral("Описание предыстории:");
            noteLines << character->backgroundDescription.simplified();
        }
        notes = noteLines.join(QStringLiteral("\n"));
    }

    pdfDrawTextBlock(
        painter,
        QRectF(30, 1095, 540, 430),
        QStringLiteral("Заметки персонажа"),
        notes,
        8);

    QStringList backgroundLines;
    if (character) {
        backgroundLines << QStringLiteral("Предыстория: %1").arg(pdfDashIfEmpty(character->background));
        if (!character->backgroundFeatureName.simplified().isEmpty()) {
            backgroundLines << QString();
            backgroundLines << QStringLiteral("Умение предыстории: %1").arg(character->backgroundFeatureName.simplified());
        }
        if (!character->backgroundFeatureDescription.trimmed().isEmpty()) {
            backgroundLines << character->backgroundFeatureDescription.trimmed();
        }
        if (!character->backgroundDescription.trimmed().isEmpty()) {
            backgroundLines << QString();
            backgroundLines << character->backgroundDescription.trimmed();
        }
    }

    pdfDrawTextBlock(
        painter,
        QRectF(590, 1050, 570, 475),
        QStringLiteral("Предыстория"),
        backgroundLines.join(QStringLiteral("\n")),
        8);

    pdfDrawFooter(painter, 2);
}

void pdfDrawSpellSheetPage(QPainter &painter, const Character *character)
{
    pdfPreparePage(painter);
    pdfDrawPageTitle(painter, QStringLiteral("Лист заклинаний"));

    const QStringList castingClasses = pdfSpellcastingClassNames(character);
    const QString abilityName = pdfBestSpellcastingAbilityName(character);
    const int abilityScore = pdfSpellcastingAbilityScore(character, abilityName);
    const int abilityModifier = Character::abilityModifier(abilityScore);
    const int spellSaveDc = character ? 8 + character->proficiencyBonus + abilityModifier : 8;
    const int spellAttackBonus = character ? character->proficiencyBonus + abilityModifier : 0;

    pdfDrawPanel(painter, QRectF(30, 74, 1130, 165), QString());
    pdfDrawField(
        painter,
        QRectF(55, 100, 360, 60),
        QStringLiteral("Класс заклинателя"),
        castingClasses.isEmpty() ? pdfClassSummary(character) : castingClasses.join(QStringLiteral(" / ")),
        9);
    pdfDrawField(
        painter,
        QRectF(450, 100, 210, 60),
        QStringLiteral("Базовая характеристика"),
        pdfSpellcastingStatText(character),
        10);
    pdfDrawField(
        painter,
        QRectF(695, 100, 190, 60),
        QStringLiteral("Сл спасброска"),
        QString::number(spellSaveDc),
        13);
    pdfDrawField(
        painter,
        QRectF(920, 100, 190, 60),
        QStringLiteral("Бонус атаки"),
        pdfSignedValue(spellAttackBonus),
        13);

    pdfDrawLineList(
        painter,
        QRectF(55, 175, 1055, 70),
        QStringLiteral("Ячейки заклинаний"),
        pdfSpellSlotLines(character).isEmpty()
            ? QStringList{QStringLiteral("На текущих уровнях расходуемые ячейки не заполнены.")}
            : pdfSpellSlotLines(character),
        7);

    const QMap<int, QStringList> groupedSpells = pdfGroupedSpellNames(character);

    const qreal leftX = 45;
    const qreal middleX = 430;
    const qreal rightX = 815;
    const qreal columnWidth = 330;

    pdfDrawSpellLevelBlock(painter, QRectF(leftX, 270, columnWidth, 300), 0, groupedSpells.value(0));
    pdfDrawSpellLevelBlock(painter, QRectF(leftX, 600, columnWidth, 390), 1, groupedSpells.value(1));
    pdfDrawSpellLevelBlock(painter, QRectF(leftX, 1020, columnWidth, 420), 2, groupedSpells.value(2));

    pdfDrawSpellLevelBlock(painter, QRectF(middleX, 270, columnWidth, 390), 3, groupedSpells.value(3));
    pdfDrawSpellLevelBlock(painter, QRectF(middleX, 690, columnWidth, 390), 4, groupedSpells.value(4));
    pdfDrawSpellLevelBlock(painter, QRectF(middleX, 1110, columnWidth, 330), 5, groupedSpells.value(5));

    pdfDrawSpellLevelBlock(painter, QRectF(rightX, 270, columnWidth, 270), 6, groupedSpells.value(6));
    pdfDrawSpellLevelBlock(painter, QRectF(rightX, 570, columnWidth, 270), 7, groupedSpells.value(7));
    pdfDrawSpellLevelBlock(painter, QRectF(rightX, 870, columnWidth, 270), 8, groupedSpells.value(8));
    pdfDrawSpellLevelBlock(painter, QRectF(rightX, 1170, columnWidth, 270), 9, groupedSpells.value(9));

    pdfDrawFooter(painter, 3);
}

}

PlayerPage::PlayerPage(QWidget *parent)
    : QWidget(parent),
      currentCharacter(nullptr),
      targetCharacterLevel(1),
      allocatedClassLevels(0)
{
    setupUi();
    connect(characterSheet, &CharacterSheet::characterUpdated, this, &PlayerPage::saveCurrentCharacter);
}

void PlayerPage::setCampaign(const QString &campaignName)
{
    currentCampaign = campaignName.trimmed();
    const QString notesScope = QStringLiteral("player_%1")
                                   .arg(currentCampaign.isEmpty() ? QStringLiteral("default") : currentCampaign);
    notesWidget->setStorageScope(notesScope);
    loadCharacterForCurrentCampaign();
}

void PlayerPage::setupUi()
{
    setObjectName(QStringLiteral("PlayerPage"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    tabWidget = new QTabWidget(this);
    tabWidget->setDocumentMode(true);

    QToolButton *menuBtn = new QToolButton(this);
    menuBtn->setObjectName(QStringLiteral("SidebarMenuBtn"));
    menuBtn->setText(QStringLiteral("\u2630"));
    menuBtn->setAutoRaise(true);
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    menuBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    QFont btnFont = menuBtn->font();
    btnFont.setPointSize(13);
    btnFont.setBold(true);
    menuBtn->setFont(btnFont);

    QMenu *menu = new QMenu(menuBtn);
    QAction *referenceMaterialsAction = menu->addAction(QStringLiteral("Справочный материал"));
    menu->addSeparator();
    QAction *mainMenuAction = menu->addAction(QStringLiteral("Главное меню"));

    menuBtn->setMenu(menu);
    connect(referenceMaterialsAction, &QAction::triggered, this, [this]() {
        showReferenceMaterialsDialog(this);
    });
    connect(mainMenuAction, &QAction::triggered, this, &PlayerPage::mainMenuRequested);

    tabWidget->setCornerWidget(menuBtn, Qt::TopRightCorner);

    notesWidget = new NotesWidget(this);

    QWidget *charTab = new QWidget();
    QVBoxLayout *charLayout = new QVBoxLayout(charTab);
    charLayout->setContentsMargins(8, 8, 8, 8);
    charLayout->setSpacing(8);

    charStack = new QStackedWidget(charTab);

    QWidget *charInfoPage = new QWidget();
    QVBoxLayout *infoLayout = new QVBoxLayout(charInfoPage);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(8);

    QHBoxLayout *charControlsLayout = new QHBoxLayout();
    QLabel *charHeaderLabel = new QLabel(QStringLiteral("Управление персонажем"));
    QFont headerFont = charHeaderLabel->font();
    headerFont.setPointSize(13);
    headerFont.setBold(true);
    charHeaderLabel->setFont(headerFont);

    QPushButton *createCharBtn = new QPushButton(QStringLiteral("Создать нового персонажа"));
    createCharBtn->setProperty("variant", QStringLiteral("accent"));
    QPushButton *levelUpBtn = new QPushButton(QStringLiteral("Повысить уровень"));
    connect(createCharBtn, &QPushButton::clicked, this, &PlayerPage::startCharacterCreation);
    connect(levelUpBtn, &QPushButton::clicked, this, &PlayerPage::levelUpCharacter);

    charControlsLayout->addWidget(charHeaderLabel);
    charControlsLayout->addStretch();
    charControlsLayout->addWidget(levelUpBtn);
    charControlsLayout->addWidget(createCharBtn);

    infoLayout->addLayout(charControlsLayout);

    QScrollArea *characterSheetScroll = new QScrollArea(charInfoPage);
    characterSheetScroll->setWidgetResizable(true);
    characterSheetScroll->setFrameShape(QFrame::NoFrame);
    characterSheetScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    characterSheetScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    characterSheet = new CharacterSheet(characterSheetScroll);
    characterSheetScroll->setWidget(characterSheet);
    infoLayout->addWidget(characterSheetScroll, 1);

    QPushButton *exportPdfBtn = new QPushButton(QStringLiteral("Экспорт в PDF"));
    connect(exportPdfBtn, &QPushButton::clicked, this, &PlayerPage::exportCurrentCharacterToPdf);
    infoLayout->addWidget(exportPdfBtn, 0, Qt::AlignRight);

    charStack->addWidget(charInfoPage);

    QWidget *creationPage = new QWidget();
    QVBoxLayout *creationLayout = new QVBoxLayout(creationPage);
    creationLayout->setContentsMargins(0, 0, 0, 0);
    creationLayout->setSpacing(8);

    QHBoxLayout *creationHeader = new QHBoxLayout();
    QPushButton *backBtn = new QPushButton(QStringLiteral("Назад"));
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        cancelPendingLevelUp(true);
        showCharacterInfo();
    });

    QLabel *stepLabel = new QLabel(QStringLiteral("Шаг 1: выбор расы"));
    QFont stepFont = stepLabel->font();
    stepFont.setPointSize(12);
    stepFont.setBold(true);
    stepLabel->setFont(stepFont);

    creationHeader->addWidget(backBtn);
    creationHeader->addStretch();
    creationHeader->addWidget(stepLabel);
    creationHeader->addStretch();

    creationLayout->addLayout(creationHeader);

    racePage = new RaceSelectionPage(creationPage);
    connect(racePage, &RaceSelectionPage::raceChosen, this, &PlayerPage::onRaceChosen);
    creationLayout->addWidget(racePage, 1);

    charStack->addWidget(creationPage);

    QWidget *classPageContainer = new QWidget();
    QVBoxLayout *classLayout = new QVBoxLayout(classPageContainer);
    classLayout->setContentsMargins(0, 0, 0, 0);
    classLayout->setSpacing(8);

    QHBoxLayout *classHeader = new QHBoxLayout();
    QPushButton *classBackBtn = new QPushButton(QStringLiteral("Назад"));
    connect(classBackBtn, &QPushButton::clicked, this, [this]() {
        if (levelUpInProgress) {
            cancelPendingLevelUp(true);
            showCharacterInfo();
            return;
        }
        charStack->setCurrentIndex(1);
    });

    QLabel *classStepLabel = new QLabel(QStringLiteral("Шаг 2: выбор класса"));
    classStepLabel->setFont(stepFont);

    classHeader->addWidget(classBackBtn);
    classHeader->addStretch();
    classHeader->addWidget(classStepLabel);
    classHeader->addStretch();

    classLayout->addLayout(classHeader);

    classPage = new ClassSelectionPage(classPageContainer);
    connect(classPage, &ClassSelectionPage::classChosen, this, &PlayerPage::onClassChosen);
    classLayout->addWidget(classPage, 1);
    charStack->addWidget(classPageContainer);

    charLayout->addWidget(charStack, 1);

    QWidget *spellsTab = new QWidget();
    QVBoxLayout *spellsLayout = new QVBoxLayout(spellsTab);
    spellsLayout->setContentsMargins(0, 0, 0, 0);
    spellsLayout->addWidget(new SpellBookWidget(this));

    QWidget *itemsTab = new QWidget();
    QVBoxLayout *itemsLayout = new QVBoxLayout(itemsTab);
    itemsLayout->setContentsMargins(0, 0, 0, 0);
    itemsLayout->addWidget(new ItemBookWidget(ItemBookWidget::GeneralItems, this));

    QWidget *equipmentTab = new QWidget();
    QVBoxLayout *equipmentLayout = new QVBoxLayout(equipmentTab);
    equipmentLayout->setContentsMargins(0, 0, 0, 0);
    equipmentLayout->addWidget(new ItemBookWidget(ItemBookWidget::WeaponsAndArmor, this));

    tabWidget->addTab(notesWidget, QStringLiteral("Заметки"));
    tabWidget->addTab(charTab, QStringLiteral("Персонаж"));
    tabWidget->addTab(spellsTab, QStringLiteral("Список заклинаний"));
    tabWidget->addTab(itemsTab, QStringLiteral("Список предметов"));
    tabWidget->addTab(equipmentTab, QStringLiteral("Список оружия и доспехов"));

    layout->addWidget(tabWidget, 1);
}
void PlayerPage::startCharacterCreation()
{
    if (currentCharacter && !currentCharacter->name().trimmed().isEmpty()) {
        QMessageBox overwriteBox(this);
        overwriteBox.setIcon(QMessageBox::Question);
        overwriteBox.setWindowTitle(QStringLiteral("\u041f\u0435\u0440\u0435\u0437\u0430\u043f\u0438\u0441\u0430\u0442\u044c \u043f\u0435\u0440\u0441\u043e\u043d\u0430\u0436\u0430"));
        overwriteBox.setText(QStringLiteral("\u0412 \u043a\u0430\u043c\u043f\u0430\u043d\u0438\u0438 \"%1\" \u0443\u0436\u0435 \u0435\u0441\u0442\u044c \u043f\u0435\u0440\u0441\u043e\u043d\u0430\u0436. \u0421\u043e\u0437\u0434\u0430\u0442\u044c \u043d\u043e\u0432\u043e\u0433\u043e \u0438 \u0437\u0430\u043c\u0435\u043d\u0438\u0442\u044c \u0442\u0435\u043a\u0443\u0449\u0435\u0433\u043e?")
                                 .arg(currentCampaign.isEmpty() ? QStringLiteral("\u0431\u0435\u0437 \u043d\u0430\u0437\u0432\u0430\u043d\u0438\u044f") : currentCampaign));
        QPushButton *yesBtn = overwriteBox.addButton(QStringLiteral("\u0414\u0430"), QMessageBox::AcceptRole);
        overwriteBox.addButton(QStringLiteral("\u041d\u0435\u0442"), QMessageBox::RejectRole);
        overwriteBox.exec();

        if (overwriteBox.clickedButton() != yesBtn) {
            return;
        }
    }

    bool ok = false;
    int chosenLevel = QInputDialog::getInt(
        this,
        QStringLiteral("\u0423\u0440\u043e\u0432\u0435\u043d\u044c \u043f\u0435\u0440\u0441\u043e\u043d\u0430\u0436\u0430"),
        QStringLiteral("\u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435 \u0438\u0442\u043e\u0433\u043e\u0432\u044b\u0439 \u0443\u0440\u043e\u0432\u0435\u043d\u044c \u043f\u0435\u0440\u0441\u043e\u043d\u0430\u0436\u0430 (1-20):"),
        1,
        1,
        20,
        1,
        &ok);

    if (!ok) {
        return;
    }

    QString chosenName = QInputDialog::getText(
        this,
        QStringLiteral("\u0418\u043c\u044f \u043f\u0435\u0440\u0441\u043e\u043d\u0430\u0436\u0430"),
        QStringLiteral("\u0412\u0432\u0435\u0434\u0438\u0442\u0435 \u0438\u043c\u044f \u043f\u0435\u0440\u0441\u043e\u043d\u0430\u0436\u0430:"),
        QLineEdit::Normal,
        currentCharacter ? currentCharacter->name() : QString(),
        &ok).trimmed();

    if (!ok) {
        return;
    }

    if (chosenName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("\u0418\u043c\u044f \u043f\u0435\u0440\u0441\u043e\u043d\u0430\u0436\u0430"), QStringLiteral("\u0418\u043c\u044f \u043f\u0435\u0440\u0441\u043e\u043d\u0430\u0436\u0430 \u043d\u0435 \u0434\u043e\u043b\u0436\u043d\u043e \u0431\u044b\u0442\u044c \u043f\u0443\u0441\u0442\u044b\u043c."));
        return;
    }

    if (!chooseBaseAbilityScores()) {
        return;
    }

    cancelPendingLevelUp();
    targetCharacterLevel = chosenLevel;

    if (currentCharacter) {
        characterSheet->setCharacter(nullptr);
        delete currentCharacter;
        currentCharacter = nullptr;
    }

    currentCharacter = new Character(this);
    currentCharacter->setName(chosenName);
    currentCharacter->level = targetCharacterLevel;
    resetClassSelection();
    applyBaseAbilityScores();
    currentCharacter->recalculateDerivedStats(false);

    characterSheet->setCharacter(currentCharacter);
    charStack->setCurrentIndex(1);
    racePage->showList();
}


void PlayerPage::showCharacterInfo()
{
    charStack->setCurrentIndex(0);
    characterSheet->setCharacter(currentCharacter);
}

void PlayerPage::resetCreationProgress()
{
    cancelPendingLevelUp();
    baseAbilityScores.clear();
    resetClassSelection();
}

void PlayerPage::cancelPendingLevelUp(bool restoreCharacter)
{
    if (restoreCharacter && levelUpInProgress && currentCharacter && !levelUpSnapshot.isEmpty()) {
        currentCharacter->fromJson(levelUpSnapshot);
        characterSheet->setCharacter(currentCharacter);
    }

    levelUpInProgress = false;
    levelUpChoosingMulticlass = false;
    levelUpPreviousMaxHp = 0;
    levelUpPreviousFeatSlots = 0;
    levelUpSnapshot = QJsonObject();
    if (classPage) {
        classPage->clearClassFilters();
    }
    if (currentCharacter) {
        targetCharacterLevel = currentCharacter->level;
    }
}

void PlayerPage::resetClassSelection()
{
    allocatedClassLevels = 0;
    selectedClassLevels.clear();
    selectedClasses.clear();
    selectedSubclassNames.clear();
    selectedClassSkillSelections.clear();
    selectedClassFeatureChoices.clear();
    classSelectionOrder.clear();

    if (currentCharacter) {
        currentCharacter->setCharacterClass("");
        currentCharacter->classLevels.clear();
        currentCharacter->classHitDice.clear();
        currentCharacter->subclassSelections.clear();
        currentCharacter->classSkillSelections.clear();
        currentCharacter->classFeatureChoices.clear();
        currentCharacter->classOrder.clear();
        currentCharacter->savingThrowProficiencies.clear();
        currentCharacter->armorProficiencies.clear();
        currentCharacter->weaponProficiencies.clear();
        currentCharacter->hitDie = 0;
        currentCharacter->maxHp = 0;
        currentCharacter->currentHp = 0;
        currentCharacter->tempHp = 0;
        currentCharacter->spells.clear();
        currentCharacter->spellbook.clear();
    }
}

int PlayerPage::remainingLevelsToAllocate() const
{
    return qMax(0, targetCharacterLevel - allocatedClassLevels);
}

void PlayerPage::updateCharacterClassSummary()
{
    if (!currentCharacter) {
        return;
    }

    QStringList parts;
    for (const QString &className : classSelectionOrder) {
        const int classLevel = selectedClassLevels.value(className, 0);
        if (classLevel > 0) {
            const QString subclassName = selectedSubclassNames.value(className).trimmed();
            parts << (subclassName.isEmpty()
                ? QString("%1 %2").arg(className).arg(classLevel)
                : QString("%1 %2 (%3)").arg(className).arg(classLevel).arg(subclassName));
        }
    }

    currentCharacter->setCharacterClass(parts.join(" / "));
}

void PlayerPage::prepareSelectedClassesFromCharacter()
{
    selectedClassLevels = currentCharacter ? currentCharacter->classLevels : QMap<QString, int>();
    classSelectionOrder = currentCharacter ? currentCharacter->classOrder : QStringList();
    selectedSubclassNames = currentCharacter ? currentCharacter->subclassSelections : QMap<QString, QString>();
    selectedClassSkillSelections = currentCharacter ? currentCharacter->classSkillSelections : QMap<QString, QStringList>();
    selectedClassFeatureChoices = currentCharacter ? currentCharacter->classFeatureChoices : QMap<QString, QString>();

    if (classSelectionOrder.isEmpty()) {
        classSelectionOrder = selectedClassLevels.keys();
    }

    selectedClasses.clear();
    for (const QString &className : classSelectionOrder) {
        if (className.trimmed().isEmpty()) {
            continue;
        }
        Class cls = classPage->getClassData(className);
        const QString subclassName = selectedSubclassNames.value(className).trimmed();
        if (subclassNameExists(cls, subclassName)) {
            cls.selectedSubclassName = subclassName;
        }
        selectedClasses.insert(className, cls);
    }

    allocatedClassLevels = selectedClassLevels.isEmpty()
        ? (currentCharacter ? currentCharacter->level : 0)
        : sumLevels(selectedClassLevels);
}

void PlayerPage::applyRaceDerivedBenefits(const Race &race)
{
    if (!currentCharacter) {
        return;
    }

    const QMap<QString, QString> traits = normalizedRaceTraits(race.traits);
    currentCharacter->skillProficiencies = uniqueStrings(
        currentCharacter->skillProficiencies + racialSkillProficiencies(traits, currentCharacter->level));
    currentCharacter->toolProficiencies = uniqueStrings(
        currentCharacter->toolProficiencies + racialToolProficiencies(traits, currentCharacter->level));
    currentCharacter->armorProficiencies = uniqueStrings(
        currentCharacter->armorProficiencies + racialArmorProficiencies(traits, currentCharacter->level));
    currentCharacter->weaponProficiencies = uniqueStrings(
        currentCharacter->weaponProficiencies + racialWeaponProficiencies(traits, currentCharacter->level));
}

bool PlayerPage::chooseRaceGrantedSpells(const Race &race)
{
    if (!currentCharacter) {
        return false;
    }

    currentCharacter->spells = removeRaceGrantedSpells(currentCharacter->spells);
    currentCharacter->spellbook = removeRaceGrantedSpells(currentCharacter->spellbook);

    const QList<Spell> allSpells = DatabaseManager::instance().getAllSpells();

    for (auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        if (!isMarkedRaceSpellTraitTitle(it.key())) {
            continue;
        }

        if (!raceTraitAvailableAtLevel(cleanedRaceTraitTitle(it.key()), it.value(), currentCharacter->level)) {
            continue;
        }

        const QStringList spellNames = bracketedSpellNames(it.value());
        if (spellNames.isEmpty()) {
            QMessageBox::warning(
                this,
                QStringLiteral("Расовое заклинание"),
                QStringLiteral("Для расовой способности «%1» не указано название заклинания в квадратных скобках.")
                    .arg(cleanedRaceTraitTitle(it.key())));
            return false;
        }

        QStringList grantedSpellNames = spellNames;
        if (spellNames.size() > 1 && textImpliesChoice(it.value())) {
            QList<ChoiceEntry> entries;
            for (const QString &spellName : spellNames) {
                Spell matchedSpell;
                bool foundMatch = false;
                for (const Spell &spell : allSpells) {
                    if (normalizedName(spell.name) == normalizedName(spellName)) {
                        matchedSpell = spell;
                        foundMatch = true;
                        break;
                    }
                }

                entries.append({
                    spellName,
                    spellName,
                    foundMatch ? spellDetailsText(matchedSpell) : spellName,
                    false,
                    QStringLiteral("Расовые заклинания")
                });
            }

            grantedSpellNames.clear();
            if (!chooseExactEntries(
                    this,
                    QStringLiteral("Расовое заклинание: %1").arg(race.name),
                    QStringLiteral("%1\n\nВыберите %2 заклинаний для способности «%3».")
                        .arg(it.value())
                        .arg(choiceCountFromText(it.value()))
                        .arg(cleanedRaceTraitTitle(it.key())),
                    entries,
                    qMin(choiceCountFromText(it.value()), spellNames.size()),
                    &grantedSpellNames,
                    QStringLiteral("заклинаний"),
                    QStringLiteral("Все варианты"))) {
                return false;
            }
        }

        for (const QString &spellName : grantedSpellNames) {
            bool found = false;
            for (const Spell &spell : allSpells) {
                if (normalizedName(spell.name) != normalizedName(spellName)) {
                    continue;
                }

                Spell selectedSpell = spell;
                selectedSpell.selectionClass = racialSpellSelectionOwner(race.name);
                selectedSpell.source = QStringLiteral("Раса: %1").arg(race.name);
                currentCharacter->spells.append(selectedSpell);
                found = true;
                break;
            }

            if (!found) {
                QMessageBox::warning(
                    this,
                    QStringLiteral("Расовое заклинание"),
                    QStringLiteral("Не удалось найти заклинание «%1» в базе для расовой способности «%2».")
                        .arg(spellName, race.name));
                return false;
            }
        }
    }

    QString cantripTraitTitle;
    QString cantripTraitText;
    for (auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        if (isMarkedRaceSpellTraitTitle(it.key())) {
            continue;
        }
        if (it.key().contains(QStringLiteral("Заговор"), Qt::CaseInsensitive)) {
            cantripTraitTitle = it.key().trimmed();
            cantripTraitText = it.value().trimmed();
            break;
        }
    }

    if (cantripTraitText.isEmpty()) {
        return true;
    }

    const QString spellListClass = spellListClassFromTraitText(cantripTraitText);
    if (spellListClass.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Расовый заговор"),
            QStringLiteral("Не удалось определить список заклинаний для расовой способности «%1».").arg(race.name));
        return false;
    }

    QList<Spell> availableCantrips;
    for (const Spell &spell : allSpells) {
        if (spell.level == 0 && spell.classes.contains(spellListClass, Qt::CaseInsensitive)) {
            availableCantrips.append(spell);
        }
    }

    if (availableCantrips.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Расовый заговор"),
            QStringLiteral("Не удалось найти заговоры класса «%1» для выбора расовой способности.").arg(spellListClass));
        return false;
    }

    std::sort(availableCantrips.begin(), availableCantrips.end(), [](const Spell &left, const Spell &right) {
        if (left.level != right.level) {
            return left.level < right.level;
        }
        return left.name.localeAwareCompare(right.name) < 0;
    });

    QList<ChoiceEntry> entries;
    for (const Spell &spell : availableCantrips) {
        entries.append({
            spell.name,
            spellChoiceTitle(spell),
            spellDetailsText(spell),
            false,
            spellLevelLabel(spell.level)
        });
    }

    SearchableChoiceDialog dialog(
        QStringLiteral("Расовый заговор: %1").arg(race.name),
        QStringLiteral("%1\n\nВыберите один заговор из списка класса «%2».")
            .arg(cantripTraitText, spellListClass),
        entries,
        false,
        this,
        -1,
        QString(),
        QStringLiteral("Все уровни"));

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString selectedCantripName = dialog.selectedKey().trimmed();
    if (selectedCantripName.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Расовый заговор"),
            QStringLiteral("Нужно выбрать заговор для расовой способности."));
        return false;
    }

    for (const Spell &spell : availableCantrips) {
        if (spell.name != selectedCantripName) {
            continue;
        }

        Spell selectedSpell = spell;
        selectedSpell.selectionClass = racialSpellSelectionOwner(race.name);
        selectedSpell.source = QStringLiteral("Раса: %1").arg(race.name);
        currentCharacter->spells.append(selectedSpell);
        return true;
    }

    QMessageBox::warning(
        this,
        QStringLiteral("Расовый заговор"),
        QStringLiteral("Не удалось сохранить выбранный расовый заговор."));
    return false;
}

QString PlayerPage::lastTakenClassName() const
{
    for (int index = classSelectionOrder.size() - 1; index >= 0; --index) {
        const QString className = classSelectionOrder.at(index);
        if (selectedClassLevels.value(className, 0) > 0) {
            return className;
        }
    }

    for (auto it = selectedClassLevels.constBegin(); it != selectedClassLevels.constEnd(); ++it) {
        if (it.value() > 0) {
            return it.key();
        }
    }

    return QString();
}

namespace {

QStringList standardSkillNames()
{
    return {
        QStringLiteral("Атлетика"),
        QStringLiteral("Акробатика"),
        QStringLiteral("Ловкость рук"),
        QStringLiteral("Скрытность"),
        QStringLiteral("Магия"),
        QStringLiteral("История"),
        QStringLiteral("Расследование"),
        QStringLiteral("Природа"),
        QStringLiteral("Религия"),
        QStringLiteral("Восприятие"),
        QStringLiteral("Выживание"),
        QStringLiteral("Медицина"),
        QStringLiteral("Проницательность"),
        QStringLiteral("Уход за животными"),
        QStringLiteral("Выступление"),
        QStringLiteral("Запугивание"),
        QStringLiteral("Обман"),
        QStringLiteral("Убеждение")
    };
}

QString normalizedToken(QString value)
{
    return value.simplified().toLower();
}

QStringList expandedSkillPool(const QStringList &choices)
{
    QStringList pool;
    for (const QString &choice : choices) {
        const QString trimmed = choice.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        if (normalizedToken(trimmed).contains(QStringLiteral("любой навык"))) {
            pool << standardSkillNames();
        } else {
            pool << trimmed;
        }
    }

    QStringList uniquePool;
    for (const QString &skill : pool) {
        if (!uniquePool.contains(skill, Qt::CaseInsensitive)) {
            uniquePool << skill;
        }
    }
    return uniquePool;
}

int skillCountFromMulticlassEntry(const QString &entry)
{
    const QString lowered = normalizedToken(entry);
    if (!lowered.contains(QStringLiteral("навык"))) {
        return 0;
    }
    if (lowered.contains(QStringLiteral("РґРІР°")) || lowered.contains(QStringLiteral("2"))) {
        return 2;
    }
    if (lowered.contains(QStringLiteral("С‚СЂРё")) || lowered.contains(QStringLiteral("3"))) {
        return 3;
    }
    return 1;
}

QStringList skillPoolForMulticlassEntry(const Class &cls, const QString &entry)
{
    const QString lowered = normalizedToken(entry);
    if (!lowered.contains(QStringLiteral("навык"))) {
        return {};
    }

    if (lowered.contains(QStringLiteral("любой")) ||
        (lowered.contains(QStringLiteral("на ваш выбор")) && !lowered.contains(QStringLiteral("списк")))) {
        return standardSkillNames();
    }

    return expandedSkillPool(cls.skillChoices);
}

struct ClassSkillChoiceRequest {
    QStringList options;
    int count = 0;
};

ClassSkillChoiceRequest classSkillChoiceRequest(const Class &cls, bool multiclassEntry)
{
    ClassSkillChoiceRequest request;
    if (multiclassEntry) {
        for (const QString &entry : cls.multiclassProficiencies) {
            const int entryCount = skillCountFromMulticlassEntry(entry);
            if (entryCount <= 0) {
                continue;
            }
            const QStringList entryPool = skillPoolForMulticlassEntry(cls, entry);
            if (entryPool.isEmpty()) {
                continue;
            }
            request.count += entryCount;
            request.options << entryPool;
        }
        request.options = expandedSkillPool(request.options);
        return request;
    }

    request.count = cls.skillChoiceCount;
    request.options = expandedSkillPool(cls.skillChoices);
    return request;
}

QString classFeatureChoiceKey(const QString &className, const QString &featureTitle)
{
    return QStringLiteral("%1|%2").arg(className.trimmed(), featureTitle.trimmed());
}

QStringList fightingStyleOptionsFromDescription(const QString &description)
{
    QStringList options;
    const QStringList lines = description.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::KeepEmptyParts);

    auto isFightingStyleTitle = [&](int index) -> bool {
        const QString trimmed = lines.at(index).trimmed();
        if (trimmed.isEmpty() || trimmed.length() > 48) {
            return false;
        }
        if (trimmed.startsWith(QStringLiteral("Р’С‹ "), Qt::CaseInsensitive)) {
            return false;
        }
        const QString lowered = normalizedToken(trimmed);
        if (lowered.contains(QStringLiteral("выберите")) || lowered.contains(QStringLiteral("опциональный"))) {
            return false;
        }
        if (trimmed.startsWith(QStringLiteral("Пока "), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("Если "), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("Когда "), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("Р’ "), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("Начиная "), Qt::CaseInsensitive)) {
            return false;
        }

        if (index + 1 >= lines.size()) {
            return false;
        }

        const QString nextLine = lines.at(index + 1).trimmed();
        if (nextLine.isEmpty()) {
            return false;
        }
        if (normalizedToken(nextLine).contains(QStringLiteral("опциональный"))) {
            return true;
        }
        return nextLine.length() > trimmed.length();
    };

    for (int index = 0; index < lines.size(); ++index) {
        if (!isFightingStyleTitle(index)) {
            continue;
        }
        options << lines.at(index).trimmed();
    }

    options.removeDuplicates();
    return options;
}

QString fightingStyleOptionDetails(const QString &description, const QString &option)
{
    const QStringList lines = description.split(QRegularExpression(QStringLiteral("\\r?\\n")), Qt::KeepEmptyParts);
    const QStringList options = fightingStyleOptionsFromDescription(description);

    int startIndex = -1;
    for (int index = 0; index < lines.size(); ++index) {
        if (lines.at(index).trimmed().compare(option, Qt::CaseInsensitive) == 0) {
            startIndex = index;
            break;
        }
    }
    if (startIndex < 0) {
        return description.trimmed();
    }

    int endIndex = lines.size();
    for (int index = startIndex + 1; index < lines.size(); ++index) {
        const QString candidate = lines.at(index).trimmed();
        if (options.contains(candidate, Qt::CaseInsensitive)) {
            endIndex = index;
            break;
        }
    }

    QStringList blockLines;
    for (int index = startIndex; index < endIndex; ++index) {
        const QString line = lines.at(index).trimmed();
        if (!line.isEmpty()) {
            blockLines << line;
        }
    }
    return blockLines.join(QStringLiteral("\n"));
}

QString featureOptionDetails(const ClassSection &section, const QString &option)
{
    const QString normalizedTitle = normalizedToken(section.title);
    if (normalizedTitle.contains(QStringLiteral("боевой стиль"))) {
        const QString details = fightingStyleOptionDetails(section.description, option);
        if (!details.trimmed().isEmpty()) {
            return details;
        }
    }

    const QStringList blocks = section.description.split(QStringLiteral("\n\n"), Qt::SkipEmptyParts);
    for (const QString &block : blocks) {
        const QString trimmedBlock = block.trimmed();
        if (trimmedBlock.startsWith(option, Qt::CaseInsensitive)) {
            return trimmedBlock;
        }
    }
    return section.description.trimmed();
}

QStringList parseFeatureChoiceOptions(const ClassSection &section)
{
    QStringList options;
    const QString normalizedTitle = normalizedToken(section.title);

    if (normalizedTitle.contains(QStringLiteral("боевой стиль"))) {
        return fightingStyleOptionsFromDescription(section.description);
    }

    return options;
}

bool classSectionNeedsPlayerChoice(const ClassSection &section, int classLevel)
{
    if (section.levelRequirement > 0 && section.levelRequirement > classLevel) {
        return false;
    }

    const QString normalizedTitle = normalizedToken(section.title);
    if (normalizedTitle.contains(QStringLiteral("боевой стиль"))) {
        return true;
    }

    const QString normalizedDescription = normalizedToken(section.description);
    if (section.optional && normalizedDescription.contains(QStringLiteral("выберите"))) {
        return true;
    }
    if (normalizedDescription.contains(QStringLiteral("выберите один")) ||
        normalizedDescription.contains(QStringLiteral("выберите вариант")) ||
        normalizedDescription.contains(QStringLiteral("выберите один из"))) {
        return !parseFeatureChoiceOptions(section).isEmpty();
    }

    return false;
}

}

bool PlayerPage::chooseClassSkillProficiencies(Class &cls, bool multiclassEntry)
{
    if (!currentCharacter) {
        return false;
    }

    const ClassSkillChoiceRequest request = classSkillChoiceRequest(cls, multiclassEntry);
    if (request.count <= 0 || request.options.isEmpty()) {
        return true;
    }

    const QString className = cls.name;
    QStringList existing = selectedClassSkillSelections.value(className);
    if (existing.size() >= request.count) {
        return true;
    }

    QList<ChoiceEntry> entries;
    for (const QString &skill : request.options) {
        ChoiceEntry entry;
        entry.key = skill;
        entry.title = skill;
        entry.details = QStringLiteral("Владение навыком «%1».").arg(skill);
        entry.selected = existing.contains(skill, Qt::CaseInsensitive);
        entries << entry;
    }

    const QString prompt = multiclassEntry
        ? QStringLiteral("Класс: %1\nВыберите %2 навыка для мультикласса:")
              .arg(className)
              .arg(request.count)
        : QStringLiteral("Класс: %1\nВыберите %2 навыка из списка класса:")
              .arg(className)
              .arg(request.count);

    SearchableChoiceDialog dialog(
        QStringLiteral("Владение навыками"),
        prompt,
        entries,
        true,
        this,
        request.count,
        QStringLiteral("Можно выбрать не больше %1 навыков.").arg(request.count));

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QStringList selected = dialog.selectedKeys();
    if (selected.size() != request.count) {
        QMessageBox::warning(
            this,
            QStringLiteral("Владение навыками"),
            QStringLiteral("Нужно выбрать ровно %1 навыка.").arg(request.count));
        return false;
    }

    selectedClassSkillSelections[className] = selected;
    currentCharacter->classSkillSelections[className] = selected;
    return true;
}

bool PlayerPage::chooseClassFeatureChoices(const Class &cls, int classLevel)
{
    if (!currentCharacter) {
        return false;
    }

    for (const ClassSection &section : cls.featureSections) {
        if (!classSectionNeedsPlayerChoice(section, classLevel)) {
            continue;
        }

        const QString choiceKey = classFeatureChoiceKey(cls.name, section.title);
        if (!selectedClassFeatureChoices.value(choiceKey).trimmed().isEmpty()) {
            continue;
        }

        const QStringList options = parseFeatureChoiceOptions(section);
        if (options.isEmpty()) {
            continue;
        }

        QList<ChoiceEntry> entries;
        for (const QString &option : options) {
            ChoiceEntry entry;
            entry.key = option;
            entry.title = option;
            entry.details = featureOptionDetails(section, option);
            entries << entry;
        }

        SearchableChoiceDialog dialog(
            QStringLiteral("Выбор умения"),
            QStringLiteral("Класс: %1\n%2\nВыберите вариант:")
                .arg(cls.name, section.title.trimmed()),
            entries,
            false,
            this);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        const QString selected = dialog.selectedKey().trimmed();
        if (selected.isEmpty()) {
            QMessageBox::warning(
                this,
                QStringLiteral("Выбор умения"),
                QStringLiteral("Нужно выбрать вариант для умения «%1».").arg(section.title.trimmed()));
            return false;
        }

        selectedClassFeatureChoices.insert(choiceKey, selected);
        currentCharacter->classFeatureChoices.insert(choiceKey, selected);
    }

    return true;
}

bool PlayerPage::applyClassLevelChange(const Class &cls, int levelsToAdd)
{
    if (!currentCharacter || levelsToAdd <= 0 || cls.name.trimmed().isEmpty()) {
        return false;
    }

    Class selectedClass = classPage->getClassData(cls.name);
    if (selectedClass.name.trimmed().isEmpty()) {
        selectedClass = cls;
    }
    const int priorClassLevel = selectedClassLevels.value(cls.name, 0);
    const int totalClassLevel = priorClassLevel + levelsToAdd;
    const bool multiclassEntry = priorClassLevel == 0 && sumLevels(selectedClassLevels) > 0;

    if (!chooseClassSkillProficiencies(selectedClass, multiclassEntry)) {
        return false;
    }

    const QString previousSubclass = selectedSubclassNames.value(cls.name).trimmed();
    if (subclassNameExists(selectedClass, previousSubclass)) {
        selectedClass.selectedSubclassName = previousSubclass;
    }
    if (subclassChoiceLevel(selectedClass) > totalClassLevel) {
        selectedClass.selectedSubclassName.clear();
        selectedSubclassNames.remove(cls.name);
    }

    if (!chooseSubclassForClass(&selectedClass, totalClassLevel)) {
        return false;
    }

    if (!chooseClassFeatureChoices(selectedClass, totalClassLevel)) {
        return false;
    }

    selectedClassLevels[cls.name] = totalClassLevel;
    selectedClasses[cls.name] = selectedClass;
    if (!selectedClass.selectedSubclassName.trimmed().isEmpty()) {
        selectedSubclassNames[cls.name] = selectedClass.selectedSubclassName.trimmed();
    }
    if (!classSelectionOrder.contains(cls.name)) {
        classSelectionOrder << cls.name;
    }

    synchronizeCharacterFromClasses(!levelUpInProgress);
    return true;
}

void PlayerPage::levelUpCharacter()
{
    if (!currentCharacter) {
        QMessageBox::information(this, QStringLiteral("Повышение уровня"), QStringLiteral("Сначала создайте или загрузите персонажа."));
        return;
    }

    if (currentCharacter->level >= 20) {
        QMessageBox::information(this, QStringLiteral("Повышение уровня"), QStringLiteral("Персонаж уже достиг максимального 20 уровня."));
        return;
    }

    if (currentCharacter->classLevels.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Повышение уровня"), QStringLiteral("У персонажа не выбраны классы. Завершите создание персонажа полностью."));
        return;
    }

    levelUpInProgress = true;
    levelUpChoosingMulticlass = false;
    levelUpPreviousMaxHp = currentCharacter->maxHp;
    levelUpPreviousFeatSlots = totalFeatSlots(currentCharacter->classLevels);
    levelUpSnapshot = currentCharacter->toJson();
    targetCharacterLevel = qMin(20, currentCharacter->level + 1);
    prepareSelectedClassesFromCharacter();

    const QString lastClassName = lastTakenClassName();
    QStringList existingClassNames;
    for (const QString &className : classSelectionOrder) {
        if (selectedClassLevels.value(className, 0) > 0 && !existingClassNames.contains(className)) {
            existingClassNames << className;
        }
    }
    for (auto it = selectedClassLevels.constBegin(); it != selectedClassLevels.constEnd(); ++it) {
        if (it.value() > 0 && !existingClassNames.contains(it.key())) {
            existingClassNames << it.key();
        }
    }
    const bool hasMulticlass = existingClassNames.size() > 1;
    QMessageBox choiceBox(this);
    choiceBox.setIcon(QMessageBox::Question);
    choiceBox.setWindowTitle(QStringLiteral("Повышение уровня"));
    choiceBox.setText(QStringLiteral("Персонаж будет повышен до уровня %1.").arg(targetCharacterLevel));
    choiceBox.setInformativeText(QStringLiteral("Куда добавить новый уровень?"));

    QPushButton *existingClassBtn = nullptr;
    if (!lastClassName.isEmpty()) {
        existingClassBtn = choiceBox.addButton(
            hasMulticlass
                ? QStringLiteral("Повысить существующий класс")
                : QStringLiteral("Повысить «%1»").arg(lastClassName),
            QMessageBox::AcceptRole);
    }
    QPushButton *multiclassBtn = choiceBox.addButton(
        QStringLiteral("Взять новый класс (мультикласс)"),
        QMessageBox::AcceptRole);
    QPushButton *cancelBtn = choiceBox.addButton(QStringLiteral("Отмена"), QMessageBox::RejectRole);
    choiceBox.exec();

    if (choiceBox.clickedButton() == cancelBtn) {
        cancelPendingLevelUp(true);
        return;
    }

    if (existingClassBtn && choiceBox.clickedButton() == existingClassBtn) {
        QString targetClassName = lastClassName;
        if (hasMulticlass) {
            QList<ChoiceEntry> classEntries;
            for (const QString &className : existingClassNames) {
                const int classLevel = selectedClassLevels.value(className, 0);
                const QString subclassName = selectedSubclassNames.value(className).trimmed();
                const QString details = subclassName.isEmpty()
                    ? QStringLiteral("Текущий уровень класса: %1").arg(classLevel)
                    : QStringLiteral("Текущий уровень класса: %1\nПодкласс: %2").arg(classLevel).arg(subclassName);

                classEntries.append({
                    className,
                    QStringLiteral("%1 (%2)").arg(className).arg(classLevel),
                    details,
                    false,
                    QString()
                });
            }

            SearchableChoiceDialog classDialog(
                QStringLiteral("Выбор класса"),
                QStringLiteral("Выберите класс, уровень которого нужно повысить на 1."),
                classEntries,
                false,
                this,
                -1,
                QString());

            if (classDialog.exec() != QDialog::Accepted) {
                cancelPendingLevelUp(true);
                return;
            }

            targetClassName = classDialog.selectedKey().trimmed();
            if (targetClassName.isEmpty()) {
                cancelPendingLevelUp(true);
                return;
            }
        }

        Class selectedClass = selectedClasses.value(targetClassName);
        if (selectedClass.name.isEmpty()) {
            selectedClass = classPage->getClassData(targetClassName);
        }
        if (selectedClass.name.isEmpty()) {
            QMessageBox::warning(
                this,
                QStringLiteral("Повышение уровня"),
                QStringLiteral("Не удалось загрузить данные класса «%1».").arg(targetClassName));
            cancelPendingLevelUp(true);
            return;
        }

        if (!applyClassLevelChange(selectedClass, 1)) {
            cancelPendingLevelUp(true);
            return;
        }

        completeCharacterCreation();
        return;
    }

    if (choiceBox.clickedButton() == multiclassBtn) {
        levelUpChoosingMulticlass = true;
        classPage->setExcludedClassNames(selectedClassLevels.keys());
        charStack->setCurrentIndex(2);
        classPage->showList();
        QMessageBox::information(
            this,
            QStringLiteral("Мультикласс"),
            QStringLiteral("Выберите новый класс. Уровни уже взятых классов скрыты."));
        return;
    }

    cancelPendingLevelUp(true);
}

bool PlayerPage::chooseBaseAbilityScores()
{
    baseAbilityScores.clear();

    QMessageBox modeBox(this);
    modeBox.setIcon(QMessageBox::Question);
    modeBox.setWindowTitle("Распределение характеристик");
    modeBox.setText("Выберите способ определения стартовых характеристик:");

    QPushButton *presetBtn = modeBox.addButton("Стандартный массив 15, 14, 13, 12, 10, 8", QMessageBox::AcceptRole);
    QPushButton *rollBtn = modeBox.addButton("Сгенерировать 4к6, отбрасывая наименьший", QMessageBox::AcceptRole);
    QPushButton *cancelBtn = modeBox.addButton("Отмена", QMessageBox::RejectRole);
    modeBox.exec();

    if (modeBox.clickedButton() == cancelBtn) {
        return false;
    }

    const QStringList statNames = {
        "Сила", "Ловкость", "Телосложение", "Интеллект", "Мудрость", "Харизма"
    };
    QMap<QString, int> chosenScores;
    if (modeBox.clickedButton() == presetBtn) {
        QList<int> pool = {15, 14, 13, 12, 10, 8};

        for (const QString &statName : statNames) {
            QStringList options;
            for (int value : pool) {
                options << QString::number(value);
            }

            bool ok = false;
            QString selected = QInputDialog::getItem(
                this,
                "Распределение характеристик",
                QString("Выберите значение для характеристики «%1»:").arg(statName),
                options,
                0,
                false,
                &ok);

            if (!ok || selected.isEmpty()) {
                baseAbilityScores.clear();
                return false;
            }

            int score = selected.toInt();
            chosenScores[statName] = score;
            pool.removeOne(score);
        }
    } else if (modeBox.clickedButton() == rollBtn) {
        QStringList rollLines;
        for (const QString &statName : statNames) {
            int score = rollAbilityScore4d6DropLowest();
            chosenScores[statName] = score;
            rollLines << QString("%1: %2").arg(statName).arg(score);
        }

        QMessageBox::information(
            this,
            "Результаты бросков 4к6",
            QString("Сгенерированные характеристики:\n%1").arg(rollLines.join("\n")));
    } else {
        return false;
    }

    baseAbilityScores = chosenScores;
    return true;
}

void PlayerPage::applyBaseAbilityScores()
{
    if (!currentCharacter || baseAbilityScores.isEmpty()) {
        return;
    }

    currentCharacter->strength = baseAbilityScores.value("Сила", 10);
    currentCharacter->dexterity = baseAbilityScores.value("Ловкость", 10);
    currentCharacter->constitution = baseAbilityScores.value("Телосложение", 10);
    currentCharacter->intelligence = baseAbilityScores.value("Интеллект", 10);
    currentCharacter->wisdom = baseAbilityScores.value("Мудрость", 10);
    currentCharacter->charisma = baseAbilityScores.value("Харизма", 10);
}

bool PlayerPage::applyRaceAbilityBonuses(const Race &race)
{
    if (!currentCharacter) {
        return false;
    }

    QStringList abilityTexts = raceAbilityIncreaseTexts(race);
    QString selectedAbilityText;

    if (!abilityTexts.isEmpty()) {
        if (abilityTexts.size() > 1) {
            QList<ChoiceEntry> entries;
            for (int index = 0; index < abilityTexts.size(); ++index) {
                entries.append({
                    abilityTexts.at(index),
                    QString("Вариант %1").arg(index + 1),
                    abilityTexts.at(index),
                    false,
                    QString()
                });
            }

            SearchableChoiceDialog dialog(
                QString("Увеличение характеристик: %1").arg(race.name),
                QStringLiteral("У этой расы есть несколько вариантов увеличения характеристик. Сначала выберите вариант."),
                entries,
                false,
                this);

            if (dialog.exec() != QDialog::Accepted) {
                return false;
            }

            selectedAbilityText = dialog.selectedKey();
        } else {
            selectedAbilityText = abilityTexts.first();
        }
    }

    QStringList optionTexts = inlineAsiOptions(selectedAbilityText);
    if (optionTexts.size() > 1) {
        QList<ChoiceEntry> entries;
        for (int index = 0; index < optionTexts.size(); ++index) {
            entries.append({
                optionTexts.at(index),
                QString("Подвариант %1").arg(index + 1),
                optionTexts.at(index),
                false,
                QString()
            });
        }

        SearchableChoiceDialog dialog(
            QString("Подвариант ASI: %1").arg(race.name),
            QStringLiteral("Выберите один из вариантов распределения бонусов характеристик."),
            entries,
            false,
            this);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        selectedAbilityText = dialog.selectedKey();
    }

    QMap<QString, int> bonuses;
    const QString lowered = selectedAbilityText.toLower();
    if (!selectedAbilityText.isEmpty()) {
        if (lowered.contains(QStringLiteral("всех ваших характеристик")) && lowered.contains(QStringLiteral("на 1"))) {
            for (const QString &ability : allAbilityNames()) {
                bonuses[ability] += 1;
            }
        } else {
            bonuses = fixedRaceBonusesFromText(selectedAbilityText);

            if (lowered.contains(QStringLiteral("одной характеристики по вашему выбору")) &&
                lowered.contains(QStringLiteral("РЅР° 2")) &&
                lowered.contains(QStringLiteral("другой")) &&
                lowered.contains(QStringLiteral("РЅР° 1"))) {
                if (!chooseAbilityIncreaseTargets(this, race.name, QStringLiteral("Выберите характеристику для бонуса +2."), 1, 2, {}, &bonuses)) {
                    return false;
                }
                if (!chooseAbilityIncreaseTargets(this, race.name, QStringLiteral("Выберите другую характеристику для бонуса +1."), 1, 1, bonuses.keys(), &bonuses)) {
                    return false;
                }
            } else if (lowered.contains(QStringLiteral("трех различных характеристик")) || lowered.contains(QStringLiteral("трёх различных характеристик"))) {
                if (!chooseAbilityIncreaseTargets(this, race.name, QStringLiteral("Выберите три разные характеристики для бонуса +1."), 3, 1, {}, &bonuses)) {
                    return false;
                }
            } else if (lowered.contains(QStringLiteral("двух других характеристик")) && lowered.contains(QStringLiteral("на ваш выбор"))) {
                if (!chooseAbilityIncreaseTargets(this, race.name, QStringLiteral("Выберите две другие характеристики для бонуса +1."), 2, 1, bonuses.keys(), &bonuses)) {
                    return false;
                }
            } else if ((lowered.contains(QStringLiteral("двух разных характеристик")) || lowered.contains(QStringLiteral("двух различных характеристик"))) && lowered.contains(QStringLiteral("на ваш выбор"))) {
                if (!chooseAbilityIncreaseTargets(this, race.name, QStringLiteral("Выберите две разные характеристики для бонуса +1."), 2, 1, {}, &bonuses)) {
                    return false;
                }
            } else if (lowered.contains(QStringLiteral("одной другой характеристики")) && lowered.contains(QStringLiteral("на ваш выбор"))) {
                if (!chooseAbilityIncreaseTargets(this, race.name, QStringLiteral("Выберите другую характеристику для бонуса +1."), 1, 1, bonuses.keys(), &bonuses)) {
                    return false;
                }
            } else if (lowered.contains(QStringLiteral("одной характеристики")) && lowered.contains(QStringLiteral("на ваш выбор"))) {
                const QRegularExpression amountRegex(QStringLiteral("одной характеристики[^.\\n]{0,80}?на\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
                const QRegularExpressionMatch amountMatch = amountRegex.match(selectedAbilityText);
                const int amount = amountMatch.hasMatch() ? amountMatch.captured(1).toInt() : 1;
                if (!chooseAbilityIncreaseTargets(this, race.name, QString("Выберите характеристику для бонуса +%1.").arg(amount), 1, amount, {}, &bonuses)) {
                    return false;
                }
            }
        }
    }

    if (bonuses.isEmpty()) {
        if (race.abilityScoreIncrease.value(QStringLiteral("Strength"), 0) > 0) {
            bonuses[QStringLiteral("Сила")] += race.abilityScoreIncrease.value(QStringLiteral("Strength"));
        }
        if (race.abilityScoreIncrease.value(QStringLiteral("Dexterity"), 0) > 0) {
            bonuses[QStringLiteral("Ловкость")] += race.abilityScoreIncrease.value(QStringLiteral("Dexterity"));
        }
        if (race.abilityScoreIncrease.value(QStringLiteral("Constitution"), 0) > 0) {
            bonuses[QStringLiteral("Телосложение")] += race.abilityScoreIncrease.value(QStringLiteral("Constitution"));
        }
        if (race.abilityScoreIncrease.value(QStringLiteral("Intelligence"), 0) > 0) {
            bonuses[QStringLiteral("Интеллект")] += race.abilityScoreIncrease.value(QStringLiteral("Intelligence"));
        }
        if (race.abilityScoreIncrease.value(QStringLiteral("Wisdom"), 0) > 0) {
            bonuses[QStringLiteral("Мудрость")] += race.abilityScoreIncrease.value(QStringLiteral("Wisdom"));
        }
        if (race.abilityScoreIncrease.value(QStringLiteral("Charisma"), 0) > 0) {
            bonuses[QStringLiteral("Харизма")] += race.abilityScoreIncrease.value(QStringLiteral("Charisma"));
        }
    }

    for (auto it = bonuses.begin(); it != bonuses.end(); ++it) {
        applyAbilityIncrease(it.key(), it.value());
    }

    return true;
}

bool PlayerPage::resolveChosenLanguages(const QStringList &languageEntries, const QString &sourceName, const QStringList &existingLanguages, QStringList *resolvedLanguages)
{
    if (!resolvedLanguages) {
        return false;
    }

    QStringList resolved;
    const QStringList languagePool = collectAvailableLanguages();
    for (int entryIndex = 0; entryIndex < languageEntries.size(); ++entryIndex) {
        const QString trimmed = languageEntries.at(entryIndex).trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }

        if (!languageEntryImpliesChoice(trimmed)) {
            const QStringList directLanguages = extractLanguageNames(trimmed);
            for (const QString &language : directLanguages) {
                if (!containsByName(existingLanguages + resolved, language)) {
                    resolved << language;
                }
            }
            continue;
        }

        QString combinedEntry = trimmed;
        while (entryIndex + 1 < languageEntries.size()) {
            const QString nextTrimmed = languageEntries.at(entryIndex + 1).trimmed();
            if (nextTrimmed.isEmpty() || languageEntryImpliesChoice(nextTrimmed)) {
                break;
            }

            const QStringList nextLanguages = extractLanguageNames(nextTrimmed);
            if (nextLanguages.isEmpty()) {
                break;
            }

            combinedEntry += QStringLiteral(", ") + nextTrimmed;
            ++entryIndex;
        }

        const ParsedLanguageEntry parsedEntry = parseLanguageEntry(combinedEntry);
        for (const QString &language : parsedEntry.fixedLanguages) {
            if (!containsByName(existingLanguages + resolved, language)) {
                resolved << language;
            }
        }

        const int count = choiceCountFromText(combinedEntry);
        QStringList entryOptions = parsedEntry.choiceOptions;
        if (entryOptions.isEmpty()) {
            entryOptions = languagePool;
        }

        for (int index = 0; index < count; ++index) {
            QStringList availableLanguages = entryOptions;
            availableLanguages.erase(std::remove_if(availableLanguages.begin(), availableLanguages.end(), [&](const QString &language) {
                return containsByName(existingLanguages + resolved, language);
            }), availableLanguages.end());

            if (availableLanguages.isEmpty()) {
                QMessageBox::warning(
                    this,
                    QString("Языки для %1").arg(sourceName),
                    QStringLiteral("Не осталось доступных языков для выбора без дубликатов."));
                return false;
            }

            bool ok = false;
            const QString language = QInputDialog::getItem(
                this,
                QString("Язык для %1").arg(sourceName),
                QString("Выберите язык %1 из %2:").arg(index + 1).arg(count),
                availableLanguages,
                0,
                false,
                &ok).trimmed();

            if (!ok || language.isEmpty()) {
                return false;
            }

            if (containsByName(existingLanguages + resolved, language)) {
                QMessageBox::warning(
                    this,
                    QString("Языки для %1").arg(sourceName),
                    QString("Язык «%1» уже выбран и не может дублироваться.").arg(language));
                return false;
            }

            resolved << language;
        }
    }

    *resolvedLanguages = uniqueStrings(resolved);
    return true;
}

bool PlayerPage::applyBackground(const Background &background)
{
    if (!currentCharacter) {
        return false;
    }

    QStringList resolvedLanguages;
    if (!resolveChosenLanguages(background.languages, background.name, currentCharacter->languages, &resolvedLanguages)) {
        return false;
    }

    currentCharacter->background = background.name;
    currentCharacter->backgroundDescription = background.description;
    currentCharacter->backgroundFeatureName = background.featureName;
    currentCharacter->backgroundFeatureDescription = background.featureDescription;
    currentCharacter->skillProficiencies = uniqueStrings(currentCharacter->skillProficiencies + background.skillProficiencies);
    currentCharacter->toolProficiencies = uniqueStrings(currentCharacter->toolProficiencies + background.toolProficiencies);
    currentCharacter->languages = uniqueStrings(currentCharacter->languages + resolvedLanguages);

    for (const QString &entry : background.equipment) {
        addInventoryTextEntry(entry);
    }

    characterSheet->setCharacter(currentCharacter);
    return true;
}

void PlayerPage::chooseCharacterBackground()
{
    if (!currentCharacter) {
        return;
    }

    const QList<Background> backgrounds = DatabaseManager::instance().getAllBackgrounds();
    if (backgrounds.isEmpty()) {
        return;
    }

    QList<ChoiceEntry> entries;
    for (const Background &background : backgrounds) {
        if (background.name.startsWith("Адаптация предысторий", Qt::CaseInsensitive)) {
            continue;
        }
        entries.append({background.name, background.name, backgroundDetailsText(background), background.name == currentCharacter->background});
    }

    SearchableChoiceDialog dialog(
        "Выбор предыстории",
        "Выберите предысторию персонажа. В правой панели отображаются умения, владения и стартовое снаряжение.",
        entries,
        false,
        this);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString selectedBackground = dialog.selectedKey();
    for (const Background &background : backgrounds) {
        if (background.name == selectedBackground) {
            if (!applyBackground(background)) {
                return;
            }
            return;
        }
    }
}

void PlayerPage::applyAbilityIncrease(const QString &abilityName, int amount)
{
    if (!currentCharacter || amount <= 0) {
        return;
    }

    if (abilityName == "Сила") {
        currentCharacter->strength = Character::clampAbilityScore(currentCharacter->strength + amount);
    } else if (abilityName == "Ловкость") {
        currentCharacter->dexterity = Character::clampAbilityScore(currentCharacter->dexterity + amount);
    } else if (abilityName == "Телосложение") {
        currentCharacter->constitution = Character::clampAbilityScore(currentCharacter->constitution + amount);
    } else if (abilityName == "Интеллект") {
        currentCharacter->intelligence = Character::clampAbilityScore(currentCharacter->intelligence + amount);
    } else if (abilityName == "Мудрость") {
        currentCharacter->wisdom = Character::clampAbilityScore(currentCharacter->wisdom + amount);
    } else if (abilityName == "Харизма") {
        currentCharacter->charisma = Character::clampAbilityScore(currentCharacter->charisma + amount);
    }
}

bool PlayerPage::chooseAbilityScoreImprovement(const QString &sourceLabel)
{
    if (!currentCharacter) {
        return false;
    }

    QMessageBox choiceBox(this);
    choiceBox.setIcon(QMessageBox::Question);
    choiceBox.setWindowTitle(sourceLabel);
    choiceBox.setText(QStringLiteral("Выберите формат повышения характеристик для этого слота."));
    choiceBox.setInformativeText(QStringLiteral("По правилам можно либо повысить одну характеристику на 2, либо две разные на 1."));

    QPushButton *singleAbilityBtn = choiceBox.addButton(QStringLiteral("+2 к одной характеристике"), QMessageBox::AcceptRole);
    choiceBox.addButton(QStringLiteral("+1 к двум характеристикам"), QMessageBox::AcceptRole);
    QPushButton *cancelBtn = choiceBox.addButton(QStringLiteral("Отмена"), QMessageBox::RejectRole);
    choiceBox.exec();

    if (choiceBox.clickedButton() == cancelBtn) {
        return false;
    }

    QMap<QString, int> bonuses;
    const bool pickSingleAbility = choiceBox.clickedButton() == singleAbilityBtn;
    const bool ok = chooseAbilityIncreaseTargets(
        this,
        sourceLabel,
        pickSingleAbility
            ? QStringLiteral("Выберите характеристику для бонуса +2.")
            : QStringLiteral("Выберите две разные характеристики для бонуса +1."),
        pickSingleAbility ? 1 : 2,
        pickSingleAbility ? 2 : 1,
        {},
        &bonuses);

    if (!ok || bonuses.isEmpty()) {
        return false;
    }

    for (auto it = bonuses.begin(); it != bonuses.end(); ++it) {
        applyAbilityIncrease(it.key(), it.value());
    }

    currentCharacter->abilityScoreImprovementLog << QStringLiteral("%1: %2")
        .arg(sourceLabel, formatAbilityIncreaseSummary(bonuses));
    currentCharacter->recalculateDerivedStats(false);
    return true;
}

void PlayerPage::applyFeat(const Feat &feat)
{
    if (!currentCharacter) {
        return;
    }

    if (!currentCharacter->featNames.contains(feat.name)) {
        currentCharacter->featNames << feat.name;
    }

    QString summary = feat.description;
    if (!feat.benefits.isEmpty()) {
        if (!summary.isEmpty()) {
            summary += "\n\n";
        }
        summary += feat.benefits.join("\n");
    }
    currentCharacter->featDescriptions.insert(feat.name, summary);

    int increaseAmount = 0;
    QStringList candidateAbilities;
    const QString abilityText = feat.benefits.join(" ");
    const QRegularExpression amountRegex(QStringLiteral("РЅР°\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = amountRegex.match(abilityText);
    if (match.hasMatch()) {
        increaseAmount = match.captured(1).toInt();
    }

    const QStringList allAbilities = {"Сила", "Ловкость", "Телосложение", "Интеллект", "Мудрость", "Харизма"};
    for (const QString &ability : allAbilities) {
        if (abilityText.contains(ability, Qt::CaseInsensitive)) {
            candidateAbilities << ability;
        }
    }

    if (increaseAmount > 0) {
        if (candidateAbilities.isEmpty() && abilityText.contains("по вашему выбору", Qt::CaseInsensitive)) {
            candidateAbilities = allAbilities;
        }

        candidateAbilities = uniqueStrings(candidateAbilities);
        if (!candidateAbilities.isEmpty()) {
            QString selectedAbility;
            if (candidateAbilities.size() == 1) {
                selectedAbility = candidateAbilities.first();
            } else {
                bool ok = false;
                selectedAbility = QInputDialog::getItem(
                    this,
                    QString("Черта: %1").arg(feat.name),
                    QString("Выберите характеристику для бонуса +%1:").arg(increaseAmount),
                    candidateAbilities,
                    0,
                    false,
                    &ok);
                if (!ok) {
                    selectedAbility.clear();
                }
            }

            if (!selectedAbility.isEmpty()) {
                applyAbilityIncrease(selectedAbility, increaseAmount);
            }
        }
    }
}

bool PlayerPage::chooseStartingFeats()
{
    if (!currentCharacter) {
        return false;
    }

    const QString rulesError = CharacterProgressionRules::instance().loadError();
    if (!rulesError.isEmpty()) {
        QMessageBox::warning(this, "Правила прогрессии", rulesError);
        return false;
    }

    const int availableFeatSlots = totalFeatSlots(selectedClassLevels);
    if (availableFeatSlots <= 0) {
        return true;
    }

    const QList<Feat> feats = DatabaseManager::instance().getAllFeats();
    if (feats.isEmpty()) {
        return true;
    }

    int slotIndex = spentAdvancementSlots(currentCharacter);
    while (slotIndex < availableFeatSlots) {
        const QString slotLabel = advancementChoiceLabel(slotIndex + 1);

        QMessageBox choiceBox(this);
        choiceBox.setIcon(QMessageBox::Question);
        choiceBox.setWindowTitle(slotLabel);
        choiceBox.setText(QStringLiteral("Выберите, как использовать этот слот развития."));
        choiceBox.setInformativeText(QStringLiteral("Доступны два варианта: взять черту или выполнить повышение характеристик."));

        choiceBox.addButton(QStringLiteral("Взять черту"), QMessageBox::AcceptRole);
        QPushButton *asiBtn = choiceBox.addButton(QStringLiteral("Повысить характеристики"), QMessageBox::AcceptRole);
        QPushButton *cancelBtn = choiceBox.addButton(QStringLiteral("Отмена"), QMessageBox::RejectRole);
        choiceBox.exec();

        if (choiceBox.clickedButton() == cancelBtn) {
            return false;
        }

        if (choiceBox.clickedButton() == asiBtn) {
            if (!chooseAbilityScoreImprovement(slotLabel)) {
                return false;
            }
            ++slotIndex;
            continue;
        }

        QList<Feat> eligibleFeats;
        for (const Feat &feat : feats) {
            if (featSatisfiesPrerequisite(feat, currentCharacter, selectedClassLevels, currentCharacter->featNames, true)) {
                eligibleFeats.append(feat);
            }
        }

        if (eligibleFeats.isEmpty()) {
            QMessageBox::information(
                this,
                slotLabel,
                QStringLiteral("Для этого слота нет доступных черт по prerequisite. Выберите повышение характеристик."));
            if (!chooseAbilityScoreImprovement(slotLabel)) {
                return false;
            }
            ++slotIndex;
            continue;
        }

        QList<ChoiceEntry> entries;
        for (const Feat &feat : eligibleFeats) {
            entries.append({feat.name, feat.name, featDetailsText(feat), false});
        }

        SearchableChoiceDialog dialog(
            slotLabel,
            QStringLiteral("Выберите одну черту для этого слота. Недоступные по prerequisite варианты скрыты."),
            entries,
            false,
            this);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        const QString selectedFeatName = dialog.selectedKey().trimmed();
        if (selectedFeatName.isEmpty()) {
            QMessageBox::warning(this, slotLabel, QStringLiteral("Нужно выбрать черту или вернуться к выбору ASI."));
            continue;
        }

        for (const Feat &feat : eligibleFeats) {
            if (feat.name == selectedFeatName &&
                !featSatisfiesPrerequisite(feat, currentCharacter, selectedClassLevels, currentCharacter->featNames + QStringList{selectedFeatName}, false)) {
                QMessageBox::warning(
                    this,
                    "Требования к черте",
                    QString("Черта «%1» не проходит prerequisite: %2")
                        .arg(feat.name, feat.prerequisite.isEmpty() ? QStringLiteral("требование не выполнено") : feat.prerequisite));
                break;
            }

            if (feat.name == selectedFeatName) {
                applyFeat(feat);
                currentCharacter->recalculateDerivedStats(false);
                ++slotIndex;
                break;
            }
        }
    }

    return true;
}

bool PlayerPage::chooseSubclassForClass(Class *cls, int classLevel)
{
    if (!cls || cls->subclasses.isEmpty()) {
        return true;
    }

    const int choiceLevel = subclassChoiceLevel(*cls);
    if (choiceLevel <= 0 || classLevel < choiceLevel) {
        return true;
    }

    const QString currentSubclass = cls->selectedSubclassName.trimmed();
    if (subclassNameExists(*cls, currentSubclass)) {
        return true;
    }

    QList<ChoiceEntry> entries;
    for (const ClassSubclass &subclass : cls->subclasses) {
        entries.append({
            subclass.name,
            subclass.name,
            classSubclassDetailsText(subclass),
            false,
            QStringLiteral("Подклассы")
        });
    }

    SearchableChoiceDialog dialog(
        QStringLiteral("Выбор подкласса: %1").arg(cls->name),
        QStringLiteral("По правилам D&D 5e класс «%1» выбирает подкласс на %2 уровне класса. Выберите один вариант. При выборе справа показано описание и умения подкласса.")
            .arg(cls->name)
            .arg(choiceLevel),
        entries,
        false,
        this,
        -1,
        QString(),
        QStringLiteral("Все подклассы"));

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString selectedName = dialog.selectedKey().trimmed();
    if (!subclassNameExists(*cls, selectedName)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Выбор подкласса"),
            QStringLiteral("Нужно выбрать один подкласс для класса «%1».").arg(cls->name));
        return false;
    }

    cls->selectedSubclassName = selectedName;
    return true;
}

void PlayerPage::addInventoryItem(const Item &item)
{
    if (!currentCharacter || item.name.trimmed().isEmpty()) {
        return;
    }

    for (Item &existing : currentCharacter->inventory) {
        if (existing.name.compare(item.name, Qt::CaseInsensitive) == 0) {
            existing.quantity += qMax(1, item.quantity);
            return;
        }
    }

    currentCharacter->inventory.append(item);
}

void PlayerPage::addInventoryTextEntry(const QString &entry)
{
    if (!currentCharacter || entry.trimmed().isEmpty()) {
        return;
    }

    Item item;
    item.name = entry.trimmed();
    item.description = QStringLiteral("Стартовое снаряжение");
    addInventoryItem(item);
}

void PlayerPage::chooseStartingEquipment()
{
    if (!currentCharacter) {
        return;
    }

    const QList<Item> items = DatabaseManager::instance().getAllItems();
    if (items.isEmpty()) {
        return;
    }

    QList<ChoiceEntry> entries;
    for (const Item &item : items) {
        bool alreadySelected = false;
        for (const Item &existing : currentCharacter->inventory) {
            if (existing.name.compare(item.name, Qt::CaseInsensitive) == 0) {
                alreadySelected = true;
                break;
            }
        }
        entries.append({item.name, item.name, itemDetailsText(item), alreadySelected});
    }

    SearchableChoiceDialog dialog(
        "Стартовое снаряжение",
        "Снаряжение предыстории уже добавлено автоматически. При необходимости отметьте дополнительные предметы из базы.",
        entries,
        true,
        this);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QStringList selectedItemNames = dialog.selectedKeys();
    QList<Item> retainedInventory;
    for (const Item &existing : currentCharacter->inventory) {
        if (existing.description == QStringLiteral("Стартовое снаряжение") && existing.type.isEmpty()) {
            retainedInventory.append(existing);
        }
    }
    currentCharacter->inventory = retainedInventory;

    for (const Item &item : items) {
        if (selectedItemNames.contains(item.name)) {
            addInventoryItem(item);
        }
    }
}

void PlayerPage::chooseStartingSpells()
{
    if (!currentCharacter) {
        return;
    }

    const CharacterProgressionRules &rules = CharacterProgressionRules::instance();
    const QString rulesError = rules.loadError();
    if (!rulesError.isEmpty()) {
        QMessageBox::warning(this, "Правила прогрессии", rulesError);
        return;
    }

    struct ClassSpellSelection {
        SpellSelectionRule rule;
        QList<Spell> candidates;
    };

    const QList<Spell> allSpells = DatabaseManager::instance().getAllSpells();
    if (allSpells.isEmpty()) {
        return;
    }

    QList<ClassSpellSelection> spellSelections;
    for (const QString &className : classSelectionOrder) {
        const int classLevel = selectedClassLevels.value(className, 0);
        const SpellSelectionRule rule = rules.spellSelectionRuleForClass(currentCharacter, className, classLevel);
        if (!rule.isValid()) {
            continue;
        }

        QList<Spell> candidates;
        for (const Spell &spell : allSpells) {
            if (spell.level <= rule.maxSpellLevel && spell.classes.contains(className, Qt::CaseInsensitive)) {
                candidates.append(spell);
            }
        }

        if (!candidates.isEmpty()) {
            spellSelections.append({rule, candidates});
        }
    }

    const QList<Spell> preservedRaceSpells = filterRaceGrantedSpells(currentCharacter->spells);
    const QList<Spell> preservedRaceSpellbook = filterRaceGrantedSpells(currentCharacter->spellbook);
    QList<Spell> rebuiltPreparedSpells = preservedRaceSpells;
    QList<Spell> rebuiltSpellbook = preservedRaceSpellbook;

    for (const QString &className : classSelectionOrder) {
        const Class cls = selectedClasses.value(className);
        const int classLevel = selectedClassLevels.value(className, 0);
        const QStringList grantedSpellNames = subclassSpellNames(cls, classLevel);
        if (grantedSpellNames.isEmpty()) {
            continue;
        }

        for (const QString &spellName : grantedSpellNames) {
            for (const Spell &spell : allSpells) {
                if (normalizedName(spell.name) != normalizedName(spellName)) {
                    continue;
                }

                Spell selectedSpell = spell;
                selectedSpell.selectionClass = className;
                selectedSpell.source = QStringLiteral("Подкласс: %1").arg(cls.selectedSubclassName);
                rebuiltPreparedSpells.append(selectedSpell);
                break;
            }
        }
    }

    if (spellSelections.isEmpty()) {
        currentCharacter->spells = uniqueSpells(rebuiltPreparedSpells);
        currentCharacter->spellbook = uniqueSpells(rebuiltSpellbook);
        return;
    }

    const bool singleCastingClass = spellSelections.size() == 1;
    const QList<Spell> previousPreparedSpells = removeGrantedSpells(currentCharacter->spells);
    const QList<Spell> previousSpellbook = removeGrantedSpells(currentCharacter->spellbook);

    for (const ClassSpellSelection &selection : spellSelections) {
        QList<Spell> sortedCandidates = selection.candidates;
        std::sort(sortedCandidates.begin(), sortedCandidates.end(), [](const Spell &left, const Spell &right) {
            if (left.level != right.level) {
                return left.level < right.level;
            }
            return left.name.localeAwareCompare(right.name) < 0;
        });

        QStringList selectedCantripNames;
        QStringList selectedLeveledNames;

        for (const Spell &spell : previousPreparedSpells) {
            const bool belongsToClass = spell.selectionClass.compare(selection.rule.className, Qt::CaseInsensitive) == 0;
            const bool legacySingleClassSelection = spell.selectionClass.trimmed().isEmpty() &&
                                                   singleCastingClass &&
                                                   spell.classes.contains(selection.rule.className, Qt::CaseInsensitive);
            if (!belongsToClass && !legacySingleClassSelection) {
                continue;
            }

            if (spell.level == 0) {
                selectedCantripNames << spell.name;
            } else if (!selection.rule.usesSpellbook) {
                selectedLeveledNames << spell.name;
            }
        }

        if (selection.rule.usesSpellbook) {
            for (const Spell &spell : previousSpellbook) {
                if (spell.selectionClass.compare(selection.rule.className, Qt::CaseInsensitive) == 0) {
                    selectedLeveledNames << spell.name;
                }
            }

            if (selectedLeveledNames.isEmpty()) {
                for (const Spell &spell : previousPreparedSpells) {
                    const bool belongsToClass = spell.selectionClass.compare(selection.rule.className, Qt::CaseInsensitive) == 0;
                    const bool legacySingleClassSelection = spell.selectionClass.trimmed().isEmpty() &&
                                                           singleCastingClass &&
                                                           spell.classes.contains(selection.rule.className, Qt::CaseInsensitive);
                    if ((belongsToClass || legacySingleClassSelection) && spell.level > 0) {
                        selectedLeveledNames << spell.name;
                    }
                }
            }
        }

        while (true) {
            QList<ChoiceEntry> entries;
            for (const Spell &spell : sortedCandidates) {
                if (spell.level == 0) {
                    continue;
                }
                entries.append({
                    spell.name,
                    spellChoiceTitle(spell),
                    spellDetailsText(spell),
                    selectedLeveledNames.contains(spell.name),
                    spellLevelLabel(spell.level)
                });
            }

            QList<ChoiceEntry> cantripEntries;
            for (const Spell &spell : sortedCandidates) {
                if (spell.level != 0) {
                    continue;
                }
                cantripEntries.append({
                    spell.name,
                    spellChoiceTitle(spell),
                    spellDetailsText(spell),
                    selectedCantripNames.contains(spell.name),
                    spellLevelLabel(spell.level)
                });
            }

            if (!cantripEntries.isEmpty()) {
                SearchableChoiceDialog cantripDialog(
                    QString("Заговоры: %1").arg(selection.rule.className),
                    QString("Выберите %1 заговоров для класса «%2»." )
                        .arg(selection.rule.cantripLimit)
                        .arg(selection.rule.className),
                    cantripEntries,
                    true,
                    this,
                    selection.rule.cantripLimit,
                    QString("Для класса «%1» нельзя выбрать больше %2 заговоров.")
                        .arg(selection.rule.className)
                        .arg(selection.rule.cantripLimit),
                    QStringLiteral("Все уровни"));

                if (cantripDialog.exec() != QDialog::Accepted) {
                    return;
                }

                selectedCantripNames = cantripDialog.selectedKeys();
                if (selection.rule.cantripLimit > 0 && selectedCantripNames.size() != selection.rule.cantripLimit) {
                    QMessageBox::warning(
                        this,
                        "Количество заговоров",
                        QString("Для класса «%1» нужно выбрать ровно %2 заговоров. Сейчас выбрано %3.")
                            .arg(selection.rule.className)
                            .arg(selection.rule.cantripLimit)
                            .arg(selectedCantripNames.size()));
                    continue;
                }
            }

            QStringList promptLines;
            promptLines << QString("Класс: %1 %2").arg(selection.rule.className).arg(selection.rule.classLevel);
            if (selection.rule.exactLeveledLimit) {
                promptLines << QString("%1: ровно %2.").arg(selection.rule.leveledLabel, QString::number(selection.rule.leveledLimit));
            } else {
                promptLines << QString("%1: не более %2.").arg(selection.rule.leveledLabel, QString::number(selection.rule.leveledLimit));
            }
            if (selection.rule.maxSpellLevel > 0) {
                promptLines << QString("Максимальный круг заклинаний: %1.").arg(selection.rule.maxSpellLevel);
            }
            if (!selection.rule.note.isEmpty()) {
                promptLines << selection.rule.note;
            }

            SearchableChoiceDialog dialog(
                selection.rule.usesSpellbook
                    ? QString("Книга заклинаний: %1").arg(selection.rule.className)
                    : QString("Заклинания: %1").arg(selection.rule.className),
                promptLines.join("\n"),
                entries,
                true,
                this,
                selection.rule.leveledLimit,
                QString("Для класса «%1» нельзя выбрать больше %2 %3.")
                    .arg(selection.rule.className)
                    .arg(selection.rule.leveledLimit)
                    .arg(selection.rule.leveledLabel),
                QStringLiteral("Все уровни"));

            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            const QStringList selectedNames = dialog.selectedKeys();
            selectedLeveledNames.clear();
            int selectedLeveled = 0;
            QMap<int, int> selectedByLevel;

            for (const Spell &spell : sortedCandidates) {
                if (!selectedNames.contains(spell.name)) {
                    continue;
                }

                selectedLeveledNames << spell.name;
                ++selectedLeveled;
                selectedByLevel[spell.level] += 1;
            }

            if (selection.rule.exactLeveledLimit && selectedLeveled != selection.rule.leveledLimit) {
                QMessageBox::warning(
                    this,
                    "Количество заклинаний",
                    QString("Для класса «%1» нужно выбрать ровно %2 %3. Сейчас выбрано %4.")
                        .arg(selection.rule.className)
                        .arg(selection.rule.leveledLimit)
                        .arg(selection.rule.leveledLabel)
                        .arg(selectedLeveled));
                continue;
            }

            if (!selection.rule.exactLeveledLimit && selectedLeveled > selection.rule.leveledLimit) {
                QMessageBox::warning(
                    this,
                    "Слишком много заклинаний",
                    QString("Для класса «%1» можно выбрать не более %2 %3. Сейчас выбрано %4.")
                        .arg(selection.rule.className)
                        .arg(selection.rule.leveledLimit)
                        .arg(selection.rule.leveledLabel)
                        .arg(selectedLeveled));
                continue;
            }

            bool perLevelError = false;
            for (auto it = selection.rule.perSpellLevelCaps.begin(); it != selection.rule.perSpellLevelCaps.end(); ++it) {
                if (selectedByLevel.value(it.key(), 0) > it.value()) {
                    QMessageBox::warning(
                        this,
                        "Ограничение по кругу заклинаний",
                        QString("Для класса «%1» можно выбрать не более %2 заклинаний %3 круга.")
                            .arg(selection.rule.className)
                            .arg(it.value())
                            .arg(it.key()));
                    perLevelError = true;
                    break;
                }
            }

            if (!perLevelError) {
                break;
            }
        }

        for (const Spell &spell : sortedCandidates) {
            if (spell.level == 0 && selectedCantripNames.contains(spell.name)) {
                Spell selectedSpell = spell;
                selectedSpell.selectionClass = selection.rule.className;
                rebuiltPreparedSpells.append(selectedSpell);
            }
        }

        if (selection.rule.usesSpellbook) {
            QList<Spell> wizardBook;
            for (const Spell &spell : sortedCandidates) {
                if (spell.level > 0 && selectedLeveledNames.contains(spell.name)) {
                    Spell selectedSpell = spell;
                    selectedSpell.selectionClass = selection.rule.className;
                    wizardBook.append(selectedSpell);
                    rebuiltSpellbook.append(selectedSpell);
                }
            }

            QStringList preparedNames;
            for (const Spell &spell : previousPreparedSpells) {
                const bool belongsToClass = spell.selectionClass.compare(selection.rule.className, Qt::CaseInsensitive) == 0;
                const bool legacySingleClassSelection = spell.selectionClass.trimmed().isEmpty() &&
                                                       singleCastingClass &&
                                                       spell.classes.contains(selection.rule.className, Qt::CaseInsensitive);
                if (spell.level > 0 && (belongsToClass || legacySingleClassSelection)) {
                    preparedNames << spell.name;
                }
            }

            while (true) {
                QList<ChoiceEntry> entries;
                for (const Spell &spell : wizardBook) {
                    entries.append({
                        spell.name,
                        spellChoiceTitle(spell),
                        spellDetailsText(spell),
                        preparedNames.contains(spell.name),
                        spellLevelLabel(spell.level)
                    });
                }

                SearchableChoiceDialog dialog(
                    QString("Подготовка заклинаний: %1").arg(selection.rule.className),
                    QString("Выберите не более %1 %2 из книги заклинаний.")
                        .arg(selection.rule.preparedLimit)
                        .arg(selection.rule.preparedLabel),
                    entries,
                    true,
                    this,
                    selection.rule.preparedLimit,
                    QString("Для класса «%1» нельзя выбрать больше %2 %3.")
                        .arg(selection.rule.className)
                        .arg(selection.rule.preparedLimit)
                        .arg(selection.rule.preparedLabel),
                    QStringLiteral("Все уровни"));

                if (dialog.exec() != QDialog::Accepted) {
                    return;
                }

                preparedNames = dialog.selectedKeys();
                if (selection.rule.exactPreparedLimit && preparedNames.size() != selection.rule.preparedLimit) {
                    QMessageBox::warning(
                        this,
                        "Количество подготовленных заклинаний",
                        QString("Для класса «%1» нужно выбрать ровно %2 %3.")
                            .arg(selection.rule.className)
                            .arg(selection.rule.preparedLimit)
                            .arg(selection.rule.preparedLabel));
                    continue;
                }

                if (!selection.rule.exactPreparedLimit && preparedNames.size() > selection.rule.preparedLimit) {
                    QMessageBox::warning(
                        this,
                        "Слишком много подготовленных заклинаний",
                        QString("Для класса «%1» можно подготовить не более %2 %3.")
                            .arg(selection.rule.className)
                            .arg(selection.rule.preparedLimit)
                            .arg(selection.rule.preparedLabel));
                    continue;
                }

                break;
            }

            for (const Spell &spell : wizardBook) {
                if (preparedNames.contains(spell.name)) {
                    rebuiltPreparedSpells.append(spell);
                }
            }
            continue;
        }

        for (const Spell &spell : sortedCandidates) {
            if (spell.level > 0 && selectedLeveledNames.contains(spell.name)) {
                Spell selectedSpell = spell;
                selectedSpell.selectionClass = selection.rule.className;
                rebuiltPreparedSpells.append(selectedSpell);
            }
        }
    }

    currentCharacter->spells = uniqueSpells(rebuiltPreparedSpells);
    currentCharacter->spellbook = uniqueSpells(rebuiltSpellbook);
}

void PlayerPage::completeCharacterCreation()
{
    if (!currentCharacter) {
        return;
    }

    const int previousMaxHp = levelUpInProgress ? levelUpPreviousMaxHp : currentCharacter->maxHp;
    Race raceSnapshot;
    raceSnapshot.name = currentCharacter->race();
    raceSnapshot.traits = currentCharacter->traits;

    if (!levelUpInProgress) {
        chooseCharacterBackground();
        if (!chooseStartingFeats()) {
            return;
        }
        chooseStartingEquipment();
    } else if (totalFeatSlots(selectedClassLevels) > levelUpPreviousFeatSlots) {
        if (!chooseStartingFeats()) {
            cancelPendingLevelUp(true);
            showCharacterInfo();
            return;
        }
    }

    chooseStartingSpells();
    currentCharacter->recalculateDerivedStats(false);
    applyRaceDerivedBenefits(raceSnapshot);

    if (levelUpInProgress) {
        const int gainedHp = qMax(0, currentCharacter->maxHp - previousMaxHp);
        currentCharacter->currentHp = qMin(currentCharacter->maxHp, currentCharacter->currentHp + gainedHp);
    }

    characterSheet->setCharacter(currentCharacter);
    showCharacterInfo();
    saveCurrentCharacter();

    const bool completedLevelUp = levelUpInProgress;
    cancelPendingLevelUp();

    if (completedLevelUp) {
        QMessageBox::information(
            this,
            QStringLiteral("Уровень повышен"),
            QStringLiteral("Персонаж повышен до %1 уровня. Текущее значение HP обновлено с учётом прироста максимума.")
                .arg(currentCharacter->level));
        return;
    }

    QString completionMessage = QString(
        "Создание персонажа завершено.\n\nПредыстория: %1\nЧерт выбрано: %2\nПредметов в инвентаре: %3\nЗаклинаний выбрано: %4")
            .arg(currentCharacter->background.isEmpty() ? QStringLiteral("не выбрана") : currentCharacter->background)
            .arg(currentCharacter->featNames.size())
            .arg(currentCharacter->inventory.size())
            .arg(currentCharacter->spells.size());
    if (!currentCharacter->spellbook.isEmpty()) {
        completionMessage += QString("\nЗаписано в книгу заклинаний: %1").arg(currentCharacter->spellbook.size());
    }

    QMessageBox::information(this, "Персонаж создан", completionMessage);
}

void PlayerPage::exportCurrentCharacterToPdf()
{
    if (!currentCharacter) {
        QMessageBox::warning(
            this,
            QStringLiteral("Экспорт в PDF"),
            QStringLiteral("Сначала создайте или загрузите персонажа."));
        return;
    }

    QString safeName = currentCharacter->name().trimmed();
    if (safeName.isEmpty()) {
        safeName = QStringLiteral("Лист персонажа");
    }
    safeName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));

    QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Экспорт персонажа в PDF"),
        QDir::home().filePath(safeName + QStringLiteral(".pdf")),
        QStringLiteral("PDF (*.pdf)"));

    if (filePath.trimmed().isEmpty()) {
        return;
    }
    if (!filePath.endsWith(QStringLiteral(".pdf"), Qt::CaseInsensitive)) {
        filePath += QStringLiteral(".pdf");
    }

    QPdfWriter writer(filePath);
    writer.setCreator(QStringLiteral("DnD Helper"));
    writer.setTitle(QStringLiteral("Лист персонажа: %1").arg(currentCharacter->name()));
    writer.setResolution(144);
    writer.setPageSize(QPageSize(QPageSize::A4));

    QPainter painter;
    if (!painter.begin(&writer)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Экспорт в PDF"),
            QStringLiteral("Не удалось создать PDF-файл:\n%1").arg(QDir::toNativeSeparators(filePath)));
        return;
    }

    pdfDrawCharacterSheetPage(painter, currentCharacter);
    writer.newPage();
    pdfDrawCharacterDetailsPage(painter, currentCharacter);
    if (!currentCharacter->spells.isEmpty() || !currentCharacter->spellbook.isEmpty()) {
        writer.newPage();
        pdfDrawSpellSheetPage(painter, currentCharacter);
    }
    painter.end();

    QMessageBox::information(
        this,
        QStringLiteral("Экспорт в PDF"),
        QStringLiteral("PDF-лист персонажа сохранён:\n%1").arg(QDir::toNativeSeparators(filePath)));
}

void PlayerPage::onRaceChosen(const Race &race)
{
    if (!currentCharacter) {
        return;
    }

    resetClassSelection();
    applyBaseAbilityScores();

    Race resolvedRace = race;
    if (!applyRaceTraitChoices(this, race, currentCharacter->level, &resolvedRace)) {
        return;
    }

    currentCharacter->setRace(resolvedRace.name);
    currentCharacter->size = resolvedRace.size;
    currentCharacter->speed = resolvedRace.speed;
    currentCharacter->flyingSpeed = resolvedRace.flyingSpeed;

    QStringList resolvedRaceLanguages;
    if (!resolveChosenLanguages(raceLanguageEntriesForSelection(resolvedRace), resolvedRace.name, {}, &resolvedRaceLanguages)) {
        return;
    }

    if (!applyRaceAbilityBonuses(resolvedRace)) {
        return;
    }

    currentCharacter->languages = resolvedRaceLanguages;

    if (!chooseRaceGrantedSpells(resolvedRace)) {
        return;
    }

    currentCharacter->traits = filteredRaceTraits(resolvedRace.traits);
    const QStringList baseSkills = currentCharacter->skillProficiencies;
    const QStringList baseTools = currentCharacter->toolProficiencies;
    const QStringList baseArmor = currentCharacter->armorProficiencies;
    const QStringList baseWeapons = currentCharacter->weaponProficiencies;
    applyRaceDerivedBenefits(resolvedRace);

    if (!applyRaceChoiceBenefits(this, resolvedRace, currentCharacter, baseSkills, baseTools, baseArmor, baseWeapons)) {
        currentCharacter->skillProficiencies = baseSkills;
        currentCharacter->toolProficiencies = baseTools;
        currentCharacter->armorProficiencies = baseArmor;
        currentCharacter->weaponProficiencies = baseWeapons;
        return;
    }

    currentCharacter->recalculateDerivedStats(false);

    qDebug() << "Character Race Selected:" << race.name;
    qDebug() << "New Stats: Str" << currentCharacter->strength << "Dex" << currentCharacter->dexterity
             << "Con" << currentCharacter->constitution << "Int" << currentCharacter->intelligence
             << "Wis" << currentCharacter->wisdom << "Cha" << currentCharacter->charisma;

    charStack->setCurrentIndex(2);
    classPage->showList();
}

void PlayerPage::synchronizeCharacterFromClasses(bool refillCurrentHp)
{
    if (!currentCharacter) {
        return;
    }

    currentCharacter->classLevels = selectedClassLevels;
    currentCharacter->classOrder = classSelectionOrder;
    currentCharacter->subclassSelections = selectedSubclassNames;
    currentCharacter->classSkillSelections = selectedClassSkillSelections;
    currentCharacter->classFeatureChoices = selectedClassFeatureChoices;
    currentCharacter->classHitDice.clear();
    currentCharacter->savingThrowProficiencies.clear();
    currentCharacter->armorProficiencies.clear();
    currentCharacter->weaponProficiencies.clear();

    const int projectedLevel = qMax(1, sumLevels(selectedClassLevels) > 0 ? sumLevels(selectedClassLevels) : targetCharacterLevel);
    const QMap<QString, QString> characterRaceTraits = normalizedRaceTraits(currentCharacter->traits);
    QStringList armorProficiencies = racialArmorProficiencies(characterRaceTraits, projectedLevel);
    QStringList weaponProficiencies = racialWeaponProficiencies(characterRaceTraits, projectedLevel);
    QStringList toolProficiencies = currentCharacter->toolProficiencies;
    QStringList skillProficiencies = currentCharacter->skillProficiencies;
    for (auto it = selectedClassSkillSelections.begin(); it != selectedClassSkillSelections.end(); ++it) {
        for (const QString &skill : it.value()) {
            skillProficiencies.removeAll(skill);
        }
    }
    bool firstClass = true;

    for (const QString &className : classSelectionOrder) {
        if (!selectedClasses.contains(className)) {
            continue;
        }

        const Class cls = selectedClasses.value(className);
        const int classLevel = selectedClassLevels.value(className, 0);
        if (classLevel <= 0) {
            continue;
        }

        currentCharacter->classHitDice.insert(className, cls.hitDie);
        if (firstClass) {
            currentCharacter->hitDie = cls.hitDie;
            currentCharacter->savingThrowProficiencies = cls.savingThrowProficiencies;
            firstClass = false;
        }

        armorProficiencies.append(cls.armorProficiencies);
        weaponProficiencies.append(cls.weaponProficiencies);
        armorProficiencies.append(subclassArmorProficiencies(cls, classLevel));
        weaponProficiencies.append(subclassWeaponProficiencies(cls, classLevel));
        toolProficiencies.append(subclassToolProficiencies(cls, classLevel));
        skillProficiencies.append(subclassSkillProficiencies(cls, classLevel));
        skillProficiencies.append(selectedClassSkillSelections.value(className));
    }

    if (firstClass) {
        currentCharacter->hitDie = 0;
    }

    currentCharacter->armorProficiencies = uniqueStrings(armorProficiencies);
    currentCharacter->weaponProficiencies = uniqueStrings(weaponProficiencies);
    currentCharacter->toolProficiencies = uniqueStrings(toolProficiencies);
    currentCharacter->skillProficiencies = uniqueStrings(skillProficiencies);

    allocatedClassLevels = sumLevels(selectedClassLevels);
    currentCharacter->level = qMax(1, allocatedClassLevels > 0 ? allocatedClassLevels : targetCharacterLevel);
    updateCharacterClassSummary();
    currentCharacter->recalculateDerivedStats(refillCurrentHp);
}

void PlayerPage::saveCurrentCharacter()
{
    if (!currentCharacter || currentCampaign.isEmpty()) {
        return;
    }

    DatabaseManager::instance().saveCharacter(currentCampaign, *currentCharacter);
}

void PlayerPage::loadCharacterForCurrentCampaign()
{
    characterSheet->setCharacter(nullptr);

    if (currentCharacter) {
        delete currentCharacter;
        currentCharacter = nullptr;
    }

    resetCreationProgress();

    if (currentCampaign.isEmpty()) {
        charStack->setCurrentIndex(0);
        return;
    }

    Character *loadedCharacter = new Character(this);
    if (!DatabaseManager::instance().loadCharacter(currentCampaign, loadedCharacter)) {
        delete loadedCharacter;
        charStack->setCurrentIndex(0);
        return;
    }

    currentCharacter = loadedCharacter;
    Race raceSnapshot;
    raceSnapshot.name = currentCharacter->race();
    raceSnapshot.traits = currentCharacter->traits;
    applyRaceDerivedBenefits(raceSnapshot);
    targetCharacterLevel = currentCharacter->level;
    prepareSelectedClassesFromCharacter();
    characterSheet->setCharacter(currentCharacter);
    charStack->setCurrentIndex(0);
}

void PlayerPage::onClassChosen(const Class &cls)
{
    if (!currentCharacter) return;

    int remaining = remainingLevelsToAllocate();
    if (remaining <= 0) {
        showCharacterInfo();
        return;
    }

    if (levelUpInProgress && levelUpChoosingMulticlass) {
        if (selectedClassLevels.contains(cls.name)) {
            QMessageBox::warning(
                this,
                QStringLiteral("Мультикласс"),
                QStringLiteral("Класс «%1» уже есть у персонажа. Чтобы повысить его уровень, выберите «Повысить существующий класс».")
                    .arg(cls.name));
            return;
        }

        if (!applyClassLevelChange(cls, 1)) {
            return;
        }

        completeCharacterCreation();
        return;
    }

    bool ok = false;
    QString prompt = QString("Класс: %1\nВыберите уровень этого класса (1-%2):")
                         .arg(cls.name)
                         .arg(remaining);
    int classLevel = QInputDialog::getInt(
        this,
        "Уровень класса",
        prompt,
        1,
        1,
        remaining,
        1,
        &ok);

    if (!ok) {
        return;
    }

    if (!applyClassLevelChange(cls, classLevel)) {
        return;
    }

    qDebug() << "Character Class Selected:" << cls.name << "Level:" << classLevel;

    remaining = remainingLevelsToAllocate();
    if (remaining > 0 && !levelUpInProgress) {
        QMessageBox choiceBox(this);
        choiceBox.setIcon(QMessageBox::Question);
        choiceBox.setWindowTitle("Распределение уровней");
        choiceBox.setText(QString("Текущий уровень персонажа: %1 из %2.\n"
                                  "Осталось распределить: %3.")
                              .arg(allocatedClassLevels)
                              .arg(targetCharacterLevel)
                              .arg(remaining));
        choiceBox.setInformativeText("Выберите: добавить ещё класс (мультикласс) или докачать текущий класс.");

        QPushButton *multiclassBtn = choiceBox.addButton("Мультикласс", QMessageBox::AcceptRole);
        QPushButton *fillCurrentBtn = choiceBox.addButton("Докачать текущий класс", QMessageBox::DestructiveRole);
        QPushButton *cancelBtn = choiceBox.addButton("Отмена", QMessageBox::RejectRole);
        choiceBox.exec();

        if (choiceBox.clickedButton() == multiclassBtn) {
            classPage->clearClassFilters();
            charStack->setCurrentIndex(2);
            classPage->showList();
            return;
        }

        if (choiceBox.clickedButton() == cancelBtn) {
            charStack->setCurrentIndex(2);
            classPage->showList();
            return;
        }

        if (choiceBox.clickedButton() == fillCurrentBtn) {
            Class classSnapshot = selectedClasses.value(cls.name, cls);
            if (!applyClassLevelChange(classSnapshot, remaining)) {
                return;
            }
        }
    }

    completeCharacterCreation();
}




