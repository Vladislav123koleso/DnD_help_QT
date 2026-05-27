#include "charactercreationservice.h"

#include "background.h"
#include "characterprogressionrules.h"
#include "charactersheet.h"
#include "databasemanager.h"
#include "feat.h"
#include "race_selection_page.h"
#include "class_selection_page.h"

#include <QComboBox>
#include <QDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>
#include <algorithm>

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
    if (lowered.contains("три")) {
        return 3;
    }
    if (lowered.contains("дв") || lowered.contains("2")) {
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
           lowered.contains(QStringLiteral(" или ")) ||
           lowered.contains(QStringLiteral("любой"));
}

QStringList explicitLanguageOptions(const QString &text)
{
    const QString lowered = normalizedName(text);
    if (lowered.contains(QStringLiteral("рекоменду"))) {
        return {};
    }

    if (lowered.contains(QStringLiteral("или")) ||
        lowered.contains(QStringLiteral("из ")) ||
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
    const int orIndex = lowered.lastIndexOf(QStringLiteral(" или "));
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
        if (lowered.contains(QStringLiteral("тяж"))) {
            appendIfMissing(QStringLiteral("Тяжёлые доспехи"));
        }
    }

    if (lowered.contains(QStringLiteral("щит"))) {
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

        if (mentionsSkills && mentionsTools && lowered.contains(QStringLiteral(" или "))) {
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
        value.remove(QRegularExpression(QStringLiteral("^(?:и|или)\\s+"), QRegularExpression::CaseInsensitiveOption));
        value.remove(QRegularExpression(QStringLiteral("\\s+(?:и|или)$"), QRegularExpression::CaseInsensitiveOption));
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
        QStringLiteral("\\(([а-яa-z])\\)\\s*(.*?)(?=(?:\\([а-яa-z]\\)\\s*)|$)"),
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
            QStringLiteral("%1[^.\\n]{0,60}?на\\s*(\\d+)").arg(pattern.pattern),
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
    if (lowered.contains(QStringLiteral("щит"))) {
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
        proficiencies << extractCategoryProficienciesFromTrait(it.value(), QStringLiteral("щит"));
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
            normalizedDescription.contains(QStringLiteral("лук")) ||
            normalizedDescription.contains(QStringLiteral("копь")) ||
            normalizedDescription.contains(QStringLiteral("трезуб")) ||
            normalizedDescription.contains(QStringLiteral("сеть")) ||
            normalizedDescription.contains(QStringLiteral("меч")) ||
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
        QString("%1\\s*[«\"]\\s*([^»\"]+?)\\s*[»\"]").arg(keywordPattern),
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
        !listContainsFragment(character->armorProficiencies, QStringLiteral("лёг")) &&
        !listContainsFragment(character->armorProficiencies, QStringLiteral("легк"))) {
        return false;
    }

    if (prerequisite.contains(QStringLiteral("Владение средними доспехами"), Qt::CaseInsensitive) &&
        !listContainsFragment(character->armorProficiencies, QStringLiteral("средн"))) {
        return false;
    }

    if (prerequisite.contains(QStringLiteral("Владение тяжёлыми доспехами"), Qt::CaseInsensitive) &&
        !listContainsFragment(character->armorProficiencies, QStringLiteral("тяж"))) {
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
    if (lowered.contains(QStringLiteral("два")) || lowered.contains(QStringLiteral("2"))) {
        return 2;
    }
    if (lowered.contains(QStringLiteral("три")) || lowered.contains(QStringLiteral("3"))) {
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
        if (trimmed.startsWith(QStringLiteral("Вы "), Qt::CaseInsensitive)) {
            return false;
        }
        const QString lowered = normalizedToken(trimmed);
        if (lowered.contains(QStringLiteral("выберите")) || lowered.contains(QStringLiteral("опциональный"))) {
            return false;
        }
        if (trimmed.startsWith(QStringLiteral("Пока "), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("Если "), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("Когда "), Qt::CaseInsensitive) ||
            trimmed.startsWith(QStringLiteral("В "), Qt::CaseInsensitive) ||
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

CharacterCreationService::CharacterCreationService(QObject *parent)
    : QObject(parent)
{
    m_racePage = new RaceSelectionPage;
    m_classPage = new ClassSelectionPage;
}

CharacterCreationService::~CharacterCreationService()
{
    delete m_racePage;
    delete m_classPage;
}

void CharacterCreationService::setParentWidget(QWidget *widget)
{
    m_parentWidget = widget;
}

void CharacterCreationService::setCharacter(Character *character)
{
    m_character = character;
}

void CharacterCreationService::setTargetLevel(int level)
{
    m_targetLevel = qBound(1, level, 20);
    if (m_character) {
        m_character->level = m_targetLevel;
    }
}

void CharacterCreationService::setBaseAbilityScores(const QMap<QString, int> &scores)
{
    m_baseAbilityScores = scores;
}

void CharacterCreationService::resetForNpc()
{
    m_levelUpInProgress = false;
    m_levelUpChoosingMulticlass = false;
}

int CharacterCreationService::remainingLevelsToAllocate() const
{
    return qMax(0, m_targetLevel - m_allocatedClassLevels);
}

void CharacterCreationService::syncAbilityScoresFromCharacter()
{
    if (!m_character) {
        return;
    }
    m_baseAbilityScores.clear();
    m_baseAbilityScores.insert(QStringLiteral("Сила"), m_character->strength);
    m_baseAbilityScores.insert(QStringLiteral("Ловкость"), m_character->dexterity);
    m_baseAbilityScores.insert(QStringLiteral("Телосложение"), m_character->constitution);
    m_baseAbilityScores.insert(QStringLiteral("Интеллект"), m_character->intelligence);
    m_baseAbilityScores.insert(QStringLiteral("Мудрость"), m_character->wisdom);
    m_baseAbilityScores.insert(QStringLiteral("Харизма"), m_character->charisma);
}

bool CharacterCreationService::applyRaceSelection(const Race &race)
{
    if (!m_character || !m_parentWidget) {
        return false;
    }

    resetClassSelection();
    applyBaseAbilityScores();

    Race resolvedRace = race;
    if (!applyRaceTraitChoices(m_parentWidget, race, m_character->level, &resolvedRace)) {
        return false;
    }

    m_character->setRace(resolvedRace.name);
    m_character->size = resolvedRace.size;
    m_character->speed = resolvedRace.speed;
    m_character->flyingSpeed = resolvedRace.flyingSpeed;

    QStringList resolvedRaceLanguages;
    if (!resolveChosenLanguages(raceLanguageEntriesForSelection(resolvedRace), resolvedRace.name, {}, &resolvedRaceLanguages)) {
        return false;
    }

    if (!applyRaceAbilityBonuses(resolvedRace)) {
        return false;
    }

    m_character->languages = resolvedRaceLanguages;

    if (!chooseRaceGrantedSpells(resolvedRace)) {
        return false;
    }

    m_character->traits = filteredRaceTraits(resolvedRace.traits);
    const QStringList baseSkills = m_character->skillProficiencies;
    const QStringList baseTools = m_character->toolProficiencies;
    const QStringList baseArmor = m_character->armorProficiencies;
    const QStringList baseWeapons = m_character->weaponProficiencies;
    applyRaceDerivedBenefits(resolvedRace);

    if (!applyRaceChoiceBenefits(m_parentWidget, resolvedRace, m_character, baseSkills, baseTools, baseArmor, baseWeapons)) {
        m_character->skillProficiencies = baseSkills;
        m_character->toolProficiencies = baseTools;
        m_character->armorProficiencies = baseArmor;
        m_character->weaponProficiencies = baseWeapons;
        return false;
    }

    m_character->recalculateDerivedStats(false);
    return true;
}

bool CharacterCreationService::showRacePicker(Race *selectedRace)
{
    if (!m_parentWidget || !selectedRace) {
        return false;
    }

    QDialog dialog(m_parentWidget);
    dialog.setWindowTitle(QStringLiteral("Выбор расы"));
    dialog.resize(980, 720);

    auto *layout = new QVBoxLayout(&dialog);
    auto *page = new RaceSelectionPage(&dialog);
    layout->addWidget(page);
    page->showList();

    Race chosen;
    bool accepted = false;
    QObject::connect(page, &RaceSelectionPage::raceChosen, &dialog, [&](const Race &race) {
        chosen = race;
        accepted = true;
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted || !accepted) {
        return false;
    }

    *selectedRace = chosen;
    return true;
}

bool CharacterCreationService::showClassPicker(Class *selectedClass)
{
    if (!m_parentWidget || !selectedClass) {
        return false;
    }

    QDialog dialog(m_parentWidget);
    dialog.setWindowTitle(QStringLiteral("Выбор класса"));
    dialog.resize(980, 720);

    auto *layout = new QVBoxLayout(&dialog);
    auto *page = new ClassSelectionPage(&dialog);
    layout->addWidget(page);
    page->clearClassFilters();
    page->showList();

    Class chosen;
    bool accepted = false;
    QObject::connect(page, &ClassSelectionPage::classChosen, &dialog, [&](const Class &cls) {
        chosen = cls;
        accepted = true;
        dialog.accept();
    });

    if (dialog.exec() != QDialog::Accepted || !accepted) {
        return false;
    }

    *selectedClass = chosen;
    return true;
}
void CharacterCreationService::resetClassSelection()
{
    m_allocatedClassLevels = 0;
    m_selectedClassLevels.clear();
    m_selectedClasses.clear();
    m_selectedSubclassNames.clear();
    m_selectedClassSkillSelections.clear();
    m_selectedClassFeatureChoices.clear();
    m_classSelectionOrder.clear();

    if (m_character) {
        m_character->setCharacterClass("");
        m_character->classLevels.clear();
        m_character->classHitDice.clear();
        m_character->subclassSelections.clear();
        m_character->classSkillSelections.clear();
        m_character->classFeatureChoices.clear();
        m_character->classOrder.clear();
        m_character->savingThrowProficiencies.clear();
        m_character->armorProficiencies.clear();
        m_character->weaponProficiencies.clear();
        m_character->hitDie = 0;
        m_character->maxHp = 0;
        m_character->currentHp = 0;
        m_character->tempHp = 0;
        m_character->spells.clear();
        m_character->spellbook.clear();
    }
}
void CharacterCreationService::updateCharacterClassSummary()
{
    if (!m_character) {
        return;
    }

    QStringList parts;
    for (const QString &className : m_classSelectionOrder) {
        const int classLevel = m_selectedClassLevels.value(className, 0);
        if (classLevel > 0) {
            const QString subclassName = m_selectedSubclassNames.value(className).trimmed();
            parts << (subclassName.isEmpty()
                ? QString("%1 %2").arg(className).arg(classLevel)
                : QString("%1 %2 (%3)").arg(className).arg(classLevel).arg(subclassName));
        }
    }

    m_character->setCharacterClass(parts.join(" / "));
}
void CharacterCreationService::applyRaceDerivedBenefits(const Race &race)
{
    if (!m_character) {
        return;
    }

    const QMap<QString, QString> traits = normalizedRaceTraits(race.traits);
    m_character->skillProficiencies = uniqueStrings(
        m_character->skillProficiencies + racialSkillProficiencies(traits, m_character->level));
    m_character->toolProficiencies = uniqueStrings(
        m_character->toolProficiencies + racialToolProficiencies(traits, m_character->level));
    m_character->armorProficiencies = uniqueStrings(
        m_character->armorProficiencies + racialArmorProficiencies(traits, m_character->level));
    m_character->weaponProficiencies = uniqueStrings(
        m_character->weaponProficiencies + racialWeaponProficiencies(traits, m_character->level));
}
bool CharacterCreationService::chooseRaceGrantedSpells(const Race &race)
{
    if (!m_character) {
        return false;
    }

    m_character->spells = removeRaceGrantedSpells(m_character->spells);
    m_character->spellbook = removeRaceGrantedSpells(m_character->spellbook);

    const QList<Spell> allSpells = DatabaseManager::instance().getAllSpells();

    for (auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        if (!isMarkedRaceSpellTraitTitle(it.key())) {
            continue;
        }

        if (!raceTraitAvailableAtLevel(cleanedRaceTraitTitle(it.key()), it.value(), m_character->level)) {
            continue;
        }

        const QStringList spellNames = bracketedSpellNames(it.value());
        if (spellNames.isEmpty()) {
            QMessageBox::warning(
                m_parentWidget,
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
                    m_parentWidget,
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
                m_character->spells.append(selectedSpell);
                found = true;
                break;
            }

            if (!found) {
                QMessageBox::warning(
                    m_parentWidget,
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
            m_parentWidget,
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
            m_parentWidget,
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
        m_parentWidget,
        -1,
        QString(),
        QStringLiteral("Все уровни"));

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString selectedCantripName = dialog.selectedKey().trimmed();
    if (selectedCantripName.isEmpty()) {
        QMessageBox::warning(
            m_parentWidget,
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
        m_character->spells.append(selectedSpell);
        return true;
    }

    QMessageBox::warning(
        m_parentWidget,
        QStringLiteral("Расовый заговор"),
        QStringLiteral("Не удалось сохранить выбранный расовый заговор."));
    return false;
}
bool CharacterCreationService::chooseClassSkillProficiencies(Class &cls, bool multiclassEntry)
{
    if (!m_character) {
        return false;
    }

    const ClassSkillChoiceRequest request = classSkillChoiceRequest(cls, multiclassEntry);
    if (request.count <= 0 || request.options.isEmpty()) {
        return true;
    }

    const QString className = cls.name;
    QStringList existing = m_selectedClassSkillSelections.value(className);
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
        m_parentWidget,
        request.count,
        QStringLiteral("Можно выбрать не больше %1 навыков.").arg(request.count));

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QStringList selected = dialog.selectedKeys();
    if (selected.size() != request.count) {
        QMessageBox::warning(
            m_parentWidget,
            QStringLiteral("Владение навыками"),
            QStringLiteral("Нужно выбрать ровно %1 навыка.").arg(request.count));
        return false;
    }

    m_selectedClassSkillSelections[className] = selected;
    m_character->classSkillSelections[className] = selected;
    return true;
}
bool CharacterCreationService::chooseClassFeatureChoices(const Class &cls, int classLevel)
{
    if (!m_character) {
        return false;
    }

    for (const ClassSection &section : cls.featureSections) {
        if (!classSectionNeedsPlayerChoice(section, classLevel)) {
            continue;
        }

        const QString choiceKey = classFeatureChoiceKey(cls.name, section.title);
        if (!m_selectedClassFeatureChoices.value(choiceKey).trimmed().isEmpty()) {
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
            m_parentWidget);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        const QString selected = dialog.selectedKey().trimmed();
        if (selected.isEmpty()) {
            QMessageBox::warning(
                m_parentWidget,
                QStringLiteral("Выбор умения"),
                QStringLiteral("Нужно выбрать вариант для умения «%1».").arg(section.title.trimmed()));
            return false;
        }

        m_selectedClassFeatureChoices.insert(choiceKey, selected);
        m_character->classFeatureChoices.insert(choiceKey, selected);
    }

    return true;
}
bool CharacterCreationService::applyClassLevelChange(const Class &cls, int levelsToAdd)
{
    if (!m_character || levelsToAdd <= 0 || cls.name.trimmed().isEmpty()) {
        return false;
    }

    Class selectedClass = m_classPage->getClassData(cls.name);
    if (selectedClass.name.trimmed().isEmpty()) {
        selectedClass = cls;
    }
    const int priorClassLevel = m_selectedClassLevels.value(cls.name, 0);
    const int totalClassLevel = priorClassLevel + levelsToAdd;
    const bool multiclassEntry = priorClassLevel == 0 && sumLevels(m_selectedClassLevels) > 0;

    if (!chooseClassSkillProficiencies(selectedClass, multiclassEntry)) {
        return false;
    }

    const QString previousSubclass = m_selectedSubclassNames.value(cls.name).trimmed();
    if (subclassNameExists(selectedClass, previousSubclass)) {
        selectedClass.selectedSubclassName = previousSubclass;
    }
    if (subclassChoiceLevel(selectedClass) > totalClassLevel) {
        selectedClass.selectedSubclassName.clear();
        m_selectedSubclassNames.remove(cls.name);
    }

    if (!chooseSubclassForClass(&selectedClass, totalClassLevel)) {
        return false;
    }

    if (!chooseClassFeatureChoices(selectedClass, totalClassLevel)) {
        return false;
    }

    m_selectedClassLevels[cls.name] = totalClassLevel;
    m_selectedClasses[cls.name] = selectedClass;
    if (!selectedClass.selectedSubclassName.trimmed().isEmpty()) {
        m_selectedSubclassNames[cls.name] = selectedClass.selectedSubclassName.trimmed();
    }
    if (!m_classSelectionOrder.contains(cls.name)) {
        m_classSelectionOrder << cls.name;
    }

    synchronizeCharacterFromClasses(!m_levelUpInProgress);
    return true;
}
bool CharacterCreationService::chooseBaseAbilityScores()
{
    m_baseAbilityScores.clear();

    QMessageBox modeBox(m_parentWidget);
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
                m_parentWidget,
                "Распределение характеристик",
                QString("Выберите значение для характеристики «%1»:").arg(statName),
                options,
                0,
                false,
                &ok);

            if (!ok || selected.isEmpty()) {
                m_baseAbilityScores.clear();
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
            m_parentWidget,
            "Результаты бросков 4к6",
            QString("Сгенерированные характеристики:\n%1").arg(rollLines.join("\n")));
    } else {
        return false;
    }

    m_baseAbilityScores = chosenScores;
    return true;
}
void CharacterCreationService::applyBaseAbilityScores()
{
    if (!m_character || m_baseAbilityScores.isEmpty()) {
        return;
    }

    m_character->strength = m_baseAbilityScores.value("Сила", 10);
    m_character->dexterity = m_baseAbilityScores.value("Ловкость", 10);
    m_character->constitution = m_baseAbilityScores.value("Телосложение", 10);
    m_character->intelligence = m_baseAbilityScores.value("Интеллект", 10);
    m_character->wisdom = m_baseAbilityScores.value("Мудрость", 10);
    m_character->charisma = m_baseAbilityScores.value("Харизма", 10);
}

void CharacterCreationService::applyAbilityIncrease(const QString &abilityName, int amount)
{
    if (!m_character || amount <= 0) {
        return;
    }

    if (abilityName == QStringLiteral("Сила")) {
        m_character->strength = Character::clampAbilityScore(m_character->strength + amount);
    } else if (abilityName == QStringLiteral("Ловкость")) {
        m_character->dexterity = Character::clampAbilityScore(m_character->dexterity + amount);
    } else if (abilityName == QStringLiteral("Телосложение")) {
        m_character->constitution = Character::clampAbilityScore(m_character->constitution + amount);
    } else if (abilityName == QStringLiteral("Интеллект")) {
        m_character->intelligence = Character::clampAbilityScore(m_character->intelligence + amount);
    } else if (abilityName == QStringLiteral("Мудрость")) {
        m_character->wisdom = Character::clampAbilityScore(m_character->wisdom + amount);
    } else if (abilityName == QStringLiteral("Харизма")) {
        m_character->charisma = Character::clampAbilityScore(m_character->charisma + amount);
    }
}

bool CharacterCreationService::applyRaceAbilityBonuses(const Race &race)
{
    if (!m_character) {
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
                m_parentWidget);

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
            m_parentWidget);

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
                lowered.contains(QStringLiteral("на 2")) &&
                lowered.contains(QStringLiteral("другой")) &&
                lowered.contains(QStringLiteral("на 1"))) {
                if (!chooseAbilityIncreaseTargets(m_parentWidget, race.name, QStringLiteral("Выберите характеристику для бонуса +2."), 1, 2, {}, &bonuses)) {
                    return false;
                }
                if (!chooseAbilityIncreaseTargets(m_parentWidget, race.name, QStringLiteral("Выберите другую характеристику для бонуса +1."), 1, 1, bonuses.keys(), &bonuses)) {
                    return false;
                }
            } else if (lowered.contains(QStringLiteral("трех различных характеристик")) || lowered.contains(QStringLiteral("трёх различных характеристик"))) {
                if (!chooseAbilityIncreaseTargets(m_parentWidget, race.name, QStringLiteral("Выберите три разные характеристики для бонуса +1."), 3, 1, {}, &bonuses)) {
                    return false;
                }
            } else if (lowered.contains(QStringLiteral("двух других характеристик")) && lowered.contains(QStringLiteral("на ваш выбор"))) {
                if (!chooseAbilityIncreaseTargets(m_parentWidget, race.name, QStringLiteral("Выберите две другие характеристики для бонуса +1."), 2, 1, bonuses.keys(), &bonuses)) {
                    return false;
                }
            } else if ((lowered.contains(QStringLiteral("двух разных характеристик")) || lowered.contains(QStringLiteral("двух различных характеристик"))) && lowered.contains(QStringLiteral("на ваш выбор"))) {
                if (!chooseAbilityIncreaseTargets(m_parentWidget, race.name, QStringLiteral("Выберите две разные характеристики для бонуса +1."), 2, 1, {}, &bonuses)) {
                    return false;
                }
            } else if (lowered.contains(QStringLiteral("одной другой характеристики")) && lowered.contains(QStringLiteral("на ваш выбор"))) {
                if (!chooseAbilityIncreaseTargets(m_parentWidget, race.name, QStringLiteral("Выберите другую характеристику для бонуса +1."), 1, 1, bonuses.keys(), &bonuses)) {
                    return false;
                }
            } else if (lowered.contains(QStringLiteral("одной характеристики")) && lowered.contains(QStringLiteral("на ваш выбор"))) {
                const QRegularExpression amountRegex(QStringLiteral("одной характеристики[^.\\n]{0,80}?на\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
                const QRegularExpressionMatch amountMatch = amountRegex.match(selectedAbilityText);
                const int amount = amountMatch.hasMatch() ? amountMatch.captured(1).toInt() : 1;
                if (!chooseAbilityIncreaseTargets(m_parentWidget, race.name, QString("Выберите характеристику для бонуса +%1.").arg(amount), 1, amount, {}, &bonuses)) {
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
bool CharacterCreationService::resolveChosenLanguages(const QStringList &languageEntries, const QString &sourceName, const QStringList &existingLanguages, QStringList *resolvedLanguages)
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
                    m_parentWidget,
                    QString("Языки для %1").arg(sourceName),
                    QStringLiteral("Не осталось доступных языков для выбора без дубликатов."));
                return false;
            }

            bool ok = false;
            const QString language = QInputDialog::getItem(
                m_parentWidget,
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
                    m_parentWidget,
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
bool CharacterCreationService::chooseSubclassForClass(Class *cls, int classLevel)
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
        m_parentWidget,
        -1,
        QString(),
        QStringLiteral("Все подклассы"));

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString selectedName = dialog.selectedKey().trimmed();
    if (!subclassNameExists(*cls, selectedName)) {
        QMessageBox::warning(
            m_parentWidget,
            QStringLiteral("Выбор подкласса"),
            QStringLiteral("Нужно выбрать один подкласс для класса «%1».").arg(cls->name));
        return false;
    }

    cls->selectedSubclassName = selectedName;
    return true;
}
void CharacterCreationService::chooseStartingSpells()
{
    if (!m_character) {
        return;
    }

    const CharacterProgressionRules &rules = CharacterProgressionRules::instance();
    const QString rulesError = rules.loadError();
    if (!rulesError.isEmpty()) {
        QMessageBox::warning(m_parentWidget, "Правила прогрессии", rulesError);
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
    for (const QString &className : m_classSelectionOrder) {
        const int classLevel = m_selectedClassLevels.value(className, 0);
        const SpellSelectionRule rule = rules.spellSelectionRuleForClass(m_character, className, classLevel);
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

    const QList<Spell> preservedRaceSpells = filterRaceGrantedSpells(m_character->spells);
    const QList<Spell> preservedRaceSpellbook = filterRaceGrantedSpells(m_character->spellbook);
    QList<Spell> rebuiltPreparedSpells = preservedRaceSpells;
    QList<Spell> rebuiltSpellbook = preservedRaceSpellbook;

    for (const QString &className : m_classSelectionOrder) {
        const Class cls = m_selectedClasses.value(className);
        const int classLevel = m_selectedClassLevels.value(className, 0);
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
        m_character->spells = uniqueSpells(rebuiltPreparedSpells);
        m_character->spellbook = uniqueSpells(rebuiltSpellbook);
        return;
    }

    const bool singleCastingClass = spellSelections.size() == 1;
    const QList<Spell> previousPreparedSpells = removeGrantedSpells(m_character->spells);
    const QList<Spell> previousSpellbook = removeGrantedSpells(m_character->spellbook);

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
                    m_parentWidget,
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
                        m_parentWidget,
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
                m_parentWidget,
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
                    m_parentWidget,
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
                    m_parentWidget,
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
                        m_parentWidget,
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
                    m_parentWidget,
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
                        m_parentWidget,
                        "Количество подготовленных заклинаний",
                        QString("Для класса «%1» нужно выбрать ровно %2 %3.")
                            .arg(selection.rule.className)
                            .arg(selection.rule.preparedLimit)
                            .arg(selection.rule.preparedLabel));
                    continue;
                }

                if (!selection.rule.exactPreparedLimit && preparedNames.size() > selection.rule.preparedLimit) {
                    QMessageBox::warning(
                        m_parentWidget,
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

    m_character->spells = uniqueSpells(rebuiltPreparedSpells);
    m_character->spellbook = uniqueSpells(rebuiltSpellbook);
}
void CharacterCreationService::synchronizeCharacterFromClasses(bool refillCurrentHp)
{
    if (!m_character) {
        return;
    }

    m_character->classLevels = m_selectedClassLevels;
    m_character->classOrder = m_classSelectionOrder;
    m_character->subclassSelections = m_selectedSubclassNames;
    m_character->classSkillSelections = m_selectedClassSkillSelections;
    m_character->classFeatureChoices = m_selectedClassFeatureChoices;
    m_character->classHitDice.clear();
    m_character->savingThrowProficiencies.clear();
    m_character->armorProficiencies.clear();
    m_character->weaponProficiencies.clear();

    const int projectedLevel = qMax(1, sumLevels(m_selectedClassLevels) > 0 ? sumLevels(m_selectedClassLevels) : m_targetLevel);
    const QMap<QString, QString> characterRaceTraits = normalizedRaceTraits(m_character->traits);
    QStringList armorProficiencies = racialArmorProficiencies(characterRaceTraits, projectedLevel);
    QStringList weaponProficiencies = racialWeaponProficiencies(characterRaceTraits, projectedLevel);
    QStringList toolProficiencies = m_character->toolProficiencies;
    QStringList skillProficiencies = m_character->skillProficiencies;
    for (auto it = m_selectedClassSkillSelections.begin(); it != m_selectedClassSkillSelections.end(); ++it) {
        for (const QString &skill : it.value()) {
            skillProficiencies.removeAll(skill);
        }
    }
    bool firstClass = true;

    for (const QString &className : m_classSelectionOrder) {
        if (!m_selectedClasses.contains(className)) {
            continue;
        }

        const Class cls = m_selectedClasses.value(className);
        const int classLevel = m_selectedClassLevels.value(className, 0);
        if (classLevel <= 0) {
            continue;
        }

        m_character->classHitDice.insert(className, cls.hitDie);
        if (firstClass) {
            m_character->hitDie = cls.hitDie;
            m_character->savingThrowProficiencies = cls.savingThrowProficiencies;
            firstClass = false;
        }

        armorProficiencies.append(cls.armorProficiencies);
        weaponProficiencies.append(cls.weaponProficiencies);
        armorProficiencies.append(subclassArmorProficiencies(cls, classLevel));
        weaponProficiencies.append(subclassWeaponProficiencies(cls, classLevel));
        toolProficiencies.append(subclassToolProficiencies(cls, classLevel));
        skillProficiencies.append(subclassSkillProficiencies(cls, classLevel));
        skillProficiencies.append(m_selectedClassSkillSelections.value(className));
    }

    if (firstClass) {
        m_character->hitDie = 0;
    }

    m_character->armorProficiencies = uniqueStrings(armorProficiencies);
    m_character->weaponProficiencies = uniqueStrings(weaponProficiencies);
    m_character->toolProficiencies = uniqueStrings(toolProficiencies);
    m_character->skillProficiencies = uniqueStrings(skillProficiencies);

    m_allocatedClassLevels = sumLevels(m_selectedClassLevels);
    m_character->level = qMax(1, m_allocatedClassLevels > 0 ? m_allocatedClassLevels : m_targetLevel);
    updateCharacterClassSummary();
    m_character->recalculateDerivedStats(refillCurrentHp);
}

bool CharacterCreationService::runCreationWizard()
{
    if (!m_character || !m_parentWidget) {
        return false;
    }

    if (!runAbilityScoreStep()) {
        return false;
    }
    if (!runRaceStep()) {
        return false;
    }
    if (!runClassStep()) {
        return false;
    }
    if (!runBackgroundAndEquipmentStep()) {
        return false;
    }
    if (!runSpellsStep()) {
        return false;
    }

    return true;
}

bool CharacterCreationService::runAbilityScoreStep()
{
    if (!m_character || !m_parentWidget) {
        return false;
    }

    resetClassSelection();
    if (!chooseBaseAbilityScores()) {
        return false;
    }

    applyBaseAbilityScores();
    m_character->recalculateDerivedStats(false);
    return true;
}

bool CharacterCreationService::runRaceStep()
{
    if (!m_character || !m_parentWidget) {
        return false;
    }

    Race race;
    if (!showRacePicker(&race)) {
        return false;
    }

    return applyRaceSelection(race);
}

bool CharacterCreationService::runClassStep()
{
    return runClassSelectionFlow();
}

bool CharacterCreationService::runBackgroundAndEquipmentStep()
{
    if (!m_character || !m_parentWidget) {
        return false;
    }

    chooseCharacterBackground();
    if (!chooseStartingFeats()) {
        return false;
    }
    chooseStartingEquipment();
    return true;
}

bool CharacterCreationService::runSpellsStep()
{
    return finalizeCreation(true);
}

bool CharacterCreationService::runClassSelectionFlow()
{
    if (!m_character || !m_parentWidget) {
        return false;
    }

    while (remainingLevelsToAllocate() > 0) {
        Class cls;
        if (!showClassPicker(&cls)) {
            return false;
        }

        int remaining = remainingLevelsToAllocate();
        if (remaining <= 0) {
            break;
        }

        if (m_levelUpInProgress && m_levelUpChoosingMulticlass) {
            if (m_selectedClassLevels.contains(cls.name)) {
                QMessageBox::warning(
                    m_parentWidget,
                    QStringLiteral("Мультикласс"),
                    QStringLiteral("Класс «%1» уже есть у персонажа. Чтобы повысить его уровень, выберите «Повысить последний класс».")
                        .arg(cls.name));
                continue;
            }

            if (!applyClassLevelChange(cls, 1)) {
                return false;
            }
            break;
        }

        bool ok = false;
        const QString prompt = QStringLiteral("Класс: %1\nВыберите уровень этого класса (1-%2):")
                                   .arg(cls.name)
                                   .arg(remaining);
        const int classLevel = QInputDialog::getInt(
            m_parentWidget,
            QStringLiteral("Уровень класса"),
            prompt,
            1,
            1,
            remaining,
            1,
            &ok);

        if (!ok) {
            return false;
        }

        if (!applyClassLevelChange(cls, classLevel)) {
            return false;
        }

        remaining = remainingLevelsToAllocate();
        if (remaining > 0 && !m_levelUpInProgress) {
            QMessageBox choiceBox(m_parentWidget);
            choiceBox.setIcon(QMessageBox::Question);
            choiceBox.setWindowTitle(QStringLiteral("Распределение уровней"));
            choiceBox.setText(QStringLiteral("Текущий уровень персонажа: %1 из %2.\n"
                                               "Осталось распределить: %3.")
                                  .arg(m_allocatedClassLevels)
                                  .arg(m_targetLevel)
                                  .arg(remaining));
            choiceBox.setInformativeText(QStringLiteral("Выберите: добавить ещё класс (мультикласс) или докачать текущий класс."));

            QPushButton *multiclassBtn = choiceBox.addButton(QStringLiteral("Мультикласс"), QMessageBox::AcceptRole);
            QPushButton *fillCurrentBtn = choiceBox.addButton(QStringLiteral("Докачать текущий класс"), QMessageBox::DestructiveRole);
            QPushButton *cancelBtn = choiceBox.addButton(QStringLiteral("Отмена"), QMessageBox::RejectRole);
            choiceBox.exec();

            if (choiceBox.clickedButton() == multiclassBtn) {
                continue;
            }

            if (choiceBox.clickedButton() == cancelBtn) {
                return false;
            }

            if (choiceBox.clickedButton() == fillCurrentBtn) {
                const Class classSnapshot = m_selectedClasses.value(cls.name, cls);
                if (!applyClassLevelChange(classSnapshot, remaining)) {
                    return false;
                }
            }
        }
    }

    return remainingLevelsToAllocate() == 0;
}

bool CharacterCreationService::finalizeCreation(bool includeSpells)
{
    if (!m_character) {
        return false;
    }

    Race raceSnapshot;
    raceSnapshot.name = m_character->race();
    raceSnapshot.traits = m_character->traits;

    if (includeSpells) {
        chooseStartingSpells();
    }

    m_character->recalculateDerivedStats(false);
    applyRaceDerivedBenefits(raceSnapshot);
    return true;
}

bool CharacterCreationService::applyBackground(const Background &background)
{
    if (!m_character) {
        return false;
    }

    QStringList resolvedLanguages;
    if (!resolveChosenLanguages(background.languages, background.name, m_character->languages, &resolvedLanguages)) {
        return false;
    }

    m_character->background = background.name;
    m_character->backgroundDescription = background.description;
    m_character->backgroundFeatureName = background.featureName;
    m_character->backgroundFeatureDescription = background.featureDescription;
    m_character->skillProficiencies = uniqueStrings(m_character->skillProficiencies + background.skillProficiencies);
    m_character->toolProficiencies = uniqueStrings(m_character->toolProficiencies + background.toolProficiencies);
    m_character->languages = uniqueStrings(m_character->languages + resolvedLanguages);

    for (const QString &entry : background.equipment) {
        addInventoryTextEntry(entry);
    }

    return true;
}

void CharacterCreationService::chooseCharacterBackground()
{
    if (!m_character || !m_parentWidget) {
        return;
    }

    const QList<Background> backgrounds = DatabaseManager::instance().getAllBackgrounds();
    if (backgrounds.isEmpty()) {
        return;
    }

    QList<ChoiceEntry> entries;
    for (const Background &background : backgrounds) {
        if (background.name.startsWith(QStringLiteral("Адаптация предысторий"), Qt::CaseInsensitive)) {
            continue;
        }
        entries.append({background.name, background.name, backgroundDetailsText(background), background.name == m_character->background});
    }

    SearchableChoiceDialog dialog(
        QStringLiteral("Выбор предыстории"),
        QStringLiteral("Выберите предысторию персонажа. В правой панели отображаются умения, владения и стартовое снаряжение."),
        entries,
        false,
        m_parentWidget);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString selectedBackground = dialog.selectedKey();
    for (const Background &background : backgrounds) {
        if (background.name == selectedBackground) {
            applyBackground(background);
            return;
        }
    }
}

bool CharacterCreationService::chooseAbilityScoreImprovement(const QString &sourceLabel)
{
    if (!m_character || !m_parentWidget) {
        return false;
    }

    QMessageBox choiceBox(m_parentWidget);
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
        m_parentWidget,
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

    m_character->abilityScoreImprovementLog << QStringLiteral("%1: %2")
        .arg(sourceLabel, formatAbilityIncreaseSummary(bonuses));
    m_character->recalculateDerivedStats(false);
    return true;
}

void CharacterCreationService::applyFeat(const Feat &feat)
{
    if (!m_character) {
        return;
    }

    if (!m_character->featNames.contains(feat.name)) {
        m_character->featNames << feat.name;
    }

    QString summary = feat.description;
    if (!feat.benefits.isEmpty()) {
        if (!summary.isEmpty()) {
            summary += QStringLiteral("\n\n");
        }
        summary += feat.benefits.join(QStringLiteral("\n"));
    }
    m_character->featDescriptions.insert(feat.name, summary);

    int increaseAmount = 0;
    QStringList candidateAbilities;
    const QString abilityText = feat.benefits.join(QStringLiteral(" "));
    const QRegularExpression amountRegex(QStringLiteral("на\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = amountRegex.match(abilityText);
    if (match.hasMatch()) {
        increaseAmount = match.captured(1).toInt();
    }

    const QStringList allAbilities = {
        QStringLiteral("Сила"),
        QStringLiteral("Ловкость"),
        QStringLiteral("Телосложение"),
        QStringLiteral("Интеллект"),
        QStringLiteral("Мудрость"),
        QStringLiteral("Харизма")
    };
    for (const QString &ability : allAbilities) {
        if (abilityText.contains(ability, Qt::CaseInsensitive)) {
            candidateAbilities << ability;
        }
    }

    if (increaseAmount > 0) {
        if (candidateAbilities.isEmpty() && abilityText.contains(QStringLiteral("по вашему выбору"), Qt::CaseInsensitive)) {
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
                    m_parentWidget,
                    QStringLiteral("Черта: %1").arg(feat.name),
                    QStringLiteral("Выберите характеристику для бонуса +%1:").arg(increaseAmount),
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

bool CharacterCreationService::chooseStartingFeats()
{
    if (!m_character || !m_parentWidget) {
        return false;
    }

    const QString rulesError = CharacterProgressionRules::instance().loadError();
    if (!rulesError.isEmpty()) {
        QMessageBox::warning(m_parentWidget, QStringLiteral("Правила прогрессии"), rulesError);
        return false;
    }

    const int availableFeatSlots = totalFeatSlots(m_selectedClassLevels);
    if (availableFeatSlots <= 0) {
        return true;
    }

    const QList<Feat> feats = DatabaseManager::instance().getAllFeats();
    if (feats.isEmpty()) {
        return true;
    }

    int slotIndex = spentAdvancementSlots(m_character);
    while (slotIndex < availableFeatSlots) {
        const QString slotLabel = advancementChoiceLabel(slotIndex + 1);

        QMessageBox choiceBox(m_parentWidget);
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
            if (featSatisfiesPrerequisite(feat, m_character, m_selectedClassLevels, m_character->featNames, true)) {
                eligibleFeats.append(feat);
            }
        }

        if (eligibleFeats.isEmpty()) {
            QMessageBox::information(
                m_parentWidget,
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
            m_parentWidget);

        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }

        const QString selectedFeatName = dialog.selectedKey().trimmed();
        if (selectedFeatName.isEmpty()) {
            QMessageBox::warning(m_parentWidget, slotLabel, QStringLiteral("Нужно выбрать черту или вернуться к выбору ASI."));
            continue;
        }

        for (const Feat &feat : eligibleFeats) {
            if (feat.name == selectedFeatName &&
                !featSatisfiesPrerequisite(feat, m_character, m_selectedClassLevels, m_character->featNames + QStringList{selectedFeatName}, false)) {
                QMessageBox::warning(
                    m_parentWidget,
                    QStringLiteral("Требования к черте"),
                    QStringLiteral("Черта «%1» не проходит prerequisite: %2")
                        .arg(feat.name, feat.prerequisite.isEmpty() ? QStringLiteral("требование не выполнено") : feat.prerequisite));
                break;
            }

            if (feat.name == selectedFeatName) {
                applyFeat(feat);
                m_character->recalculateDerivedStats(false);
                ++slotIndex;
                break;
            }
        }
    }

    return true;
}

void CharacterCreationService::addInventoryItem(const Item &item)
{
    if (!m_character || item.name.trimmed().isEmpty()) {
        return;
    }

    for (Item &existing : m_character->inventory) {
        if (existing.name.compare(item.name, Qt::CaseInsensitive) == 0) {
            existing.quantity += qMax(1, item.quantity);
            return;
        }
    }

    m_character->inventory.append(item);
}

void CharacterCreationService::addInventoryTextEntry(const QString &entry)
{
    if (!m_character || entry.trimmed().isEmpty()) {
        return;
    }

    Item item;
    item.name = entry.trimmed();
    item.description = QStringLiteral("Стартовое снаряжение");
    addInventoryItem(item);
}

void CharacterCreationService::chooseStartingEquipment()
{
    if (!m_character || !m_parentWidget) {
        return;
    }

    const QList<Item> items = DatabaseManager::instance().getAllItems();
    if (items.isEmpty()) {
        return;
    }

    QList<ChoiceEntry> entries;
    for (const Item &item : items) {
        bool alreadySelected = false;
        for (const Item &existing : m_character->inventory) {
            if (existing.name.compare(item.name, Qt::CaseInsensitive) == 0) {
                alreadySelected = true;
                break;
            }
        }
        entries.append({item.name, item.name, itemDetailsText(item), alreadySelected});
    }

    SearchableChoiceDialog dialog(
        QStringLiteral("Стартовое снаряжение"),
        QStringLiteral("Снаряжение предыстории уже добавлено автоматически. При необходимости отметьте дополнительные предметы из базы."),
        entries,
        true,
        m_parentWidget);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QStringList selectedItemNames = dialog.selectedKeys();
    QList<Item> retainedInventory;
    for (const Item &existing : m_character->inventory) {
        if (existing.description == QStringLiteral("Стартовое снаряжение") && existing.type.isEmpty()) {
            retainedInventory.append(existing);
        }
    }
    m_character->inventory = retainedInventory;

    for (const Item &item : items) {
        if (selectedItemNames.contains(item.name)) {
            addInventoryItem(item);
        }
    }
}
