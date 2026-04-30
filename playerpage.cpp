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
#include <QRegularExpression>
#include <QLineEdit>
#include <QSplitter>
#include <QTextEdit>
#include <algorithm>
#include "character.h"
#include "characterprogressionrules.h"
#include "background.h"
#include "databasemanager.h"
#include "feat.h"
#include "spellbookwidget.h"
#include "itembookwidget.h"

namespace {

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
const QStringList &knownSkillNames();
bool raceTraitAvailableAtLevel(const QString &title, const QString &description, int level);

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

bool isDisplayableRaceTraitTitle(const QString &title)
{
    if (isHiddenRaceTraitTitle(title)) {
        return false;
    }

    const QString normalizedTitle = cleanedRaceTraitTitle(title).toLower();
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
        const QString title = cleanedRaceTraitTitle(it.key());
        const QString description = cleanedRaceTraitDescription(it.value());
        if (title.isEmpty() || description.isEmpty()) {
            continue;
        }

        normalized.insert(title, description);
    }
    return normalized;
}

QMap<QString, QString> filteredRaceTraits(const QMap<QString, QString> &traits)
{
    QMap<QString, QString> filtered;
    for (auto it = traits.begin(); it != traits.end(); ++it) {
        if (isHiddenRaceTraitTitle(it.key())) {
            continue;
        }

        const QString title = cleanedRaceTraitTitle(it.key());
        const QString description = cleanedRaceTraitDescription(it.value());
        if (!isDisplayableRaceTraitTitle(title) || description.isEmpty()) {
            continue;
        }
        filtered.insert(title, description);
    }
    return filtered;
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
    loadCharacterForCurrentCampaign();
}

void PlayerPage::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    tabWidget = new QTabWidget(this);

    // Hamburger Menu Button (Corner Widget)
    QToolButton *menuBtn = new QToolButton(this);
    menuBtn->setText("☰");
    menuBtn->setAutoRaise(true);
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    menuBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    QFont btnFont = menuBtn->font();
    btnFont.setPointSize(12);
    menuBtn->setFont(btnFont);

    QMenu *menu = new QMenu(this);
    QAction *saveCharacterAction = menu->addAction("Сохранить персонажа");
    QAction *reloadCharacterAction = menu->addAction("Перезагрузить персонажа");
    menu->addSeparator();
    QAction *mainMenuAction = menu->addAction("Main Menu");

    menuBtn->setMenu(menu);
    connect(saveCharacterAction, &QAction::triggered, this, &PlayerPage::saveCurrentCharacter);
    connect(reloadCharacterAction, &QAction::triggered, this, &PlayerPage::loadCharacterForCurrentCampaign);
    connect(mainMenuAction, &QAction::triggered, this, &PlayerPage::mainMenuRequested);

    tabWidget->setCornerWidget(menuBtn, Qt::TopRightCorner);

    // 1. Notes Tab (Заметки)
    QWidget *notesTab = new QWidget();
    QVBoxLayout *notesLayout = new QVBoxLayout(notesTab);
    notesLayout->addWidget(new QLabel("Здесь будут заметки игрока"));
    notesLayout->addStretch();

    // 2. Character Tab (Персонаж)
    QWidget *charTab = new QWidget();
    QVBoxLayout *charLayout = new QVBoxLayout(charTab);
    
    charStack = new QStackedWidget(charTab);
    
    // --- Page 0: Character Info (Existing) ---
    QWidget *charInfoPage = new QWidget();
    QVBoxLayout *infoLayout = new QVBoxLayout(charInfoPage);
    
    // Top controls for character tab
    QHBoxLayout *charControlsLayout = new QHBoxLayout();
    QPushButton *createCharBtn = new QPushButton("Создать нового персонажа");
    QPushButton *levelUpBtn = new QPushButton("Повысить уровень");
    connect(createCharBtn, &QPushButton::clicked, this, &PlayerPage::startCharacterCreation);
    connect(levelUpBtn, &QPushButton::clicked, this, &PlayerPage::levelUpCharacter);
    
    charControlsLayout->addWidget(new QLabel("Информация о персонаже"));
    charControlsLayout->addStretch();
    charControlsLayout->addWidget(levelUpBtn);
    charControlsLayout->addWidget(createCharBtn);
    
    infoLayout->addLayout(charControlsLayout);
    
    // Character Sheet Widget
    characterSheet = new CharacterSheet(charInfoPage);
    infoLayout->addWidget(characterSheet);

    QPushButton *exportPdfBtn = new QPushButton("Скачать в PDF");
    infoLayout->addWidget(exportPdfBtn);
    infoLayout->addStretch();
    
    charStack->addWidget(charInfoPage);
    
    // --- Page 1: Character Creation (Race Selection) ---
    QWidget *creationPage = new QWidget();
    QVBoxLayout *creationLayout = new QVBoxLayout(creationPage);
    
    QHBoxLayout *creationHeader = new QHBoxLayout();
    QPushButton *backBtn = new QPushButton("Назад");
    connect(backBtn, &QPushButton::clicked, this, [this]() {
        cancelPendingLevelUp(true);
        showCharacterInfo();
    });
    
    QLabel *stepLabel = new QLabel("Шаг 1: Выбор расы");
    stepLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    
    creationHeader->addWidget(backBtn);
    creationHeader->addStretch();
    creationHeader->addWidget(stepLabel);
    creationHeader->addStretch();
    
    creationLayout->addLayout(creationHeader);
    
    // Race Selection Widget
    racePage = new RaceSelectionPage(creationPage);
    connect(racePage, &RaceSelectionPage::raceChosen, this, &PlayerPage::onRaceChosen);
    creationLayout->addWidget(racePage);
    
    charStack->addWidget(creationPage);
    
    // --- Page 2: Character Creation (Class Selection) ---
    QWidget *classPageContainer = new QWidget();
    QVBoxLayout *classLayout = new QVBoxLayout(classPageContainer);
    
    QHBoxLayout *classHeader = new QHBoxLayout();
    QPushButton *classBackBtn = new QPushButton("Назад");
    
    // Back from Class selection goes to Race selection (Index 1)
    connect(classBackBtn, &QPushButton::clicked, this, [this]() {
        if (levelUpInProgress) {
            cancelPendingLevelUp(true);
            showCharacterInfo();
            return;
        }
        charStack->setCurrentIndex(1);
    });
    
    QLabel *classStepLabel = new QLabel("Шаг 2: Выбор класса");
    classStepLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    
    classHeader->addWidget(classBackBtn);
    classHeader->addStretch();
    classHeader->addWidget(classStepLabel);
    classHeader->addStretch();
    
    classLayout->addLayout(classHeader);
    
    classPage = new ClassSelectionPage(classPageContainer);
    connect(classPage, &ClassSelectionPage::classChosen, this, &PlayerPage::onClassChosen);
    
    classLayout->addWidget(classPage);
    charStack->addWidget(classPageContainer);

    charLayout->addWidget(charStack);

    // 3. Spells Tab (Список Заклинаний)
    QWidget *spellsTab = new QWidget();
    QVBoxLayout *spellsLayout = new QVBoxLayout(spellsTab);
    spellsLayout->addWidget(new SpellBookWidget(this));

    // 4. Items Tab (Список Предметов)
    QWidget *itemsTab = new QWidget();
    QVBoxLayout *itemsLayout = new QVBoxLayout(itemsTab);
    itemsLayout->addWidget(new ItemBookWidget(ItemBookWidget::GeneralItems, this));

    // 5. Weapons and Armor Tab (Список Оружия и доспехов)
    QWidget *equipmentTab = new QWidget();
    QVBoxLayout *equipmentLayout = new QVBoxLayout(equipmentTab);
    equipmentLayout->addWidget(new ItemBookWidget(ItemBookWidget::WeaponsAndArmor, this));

    // Add tabs in order
    tabWidget->addTab(notesTab, "Заметки");
    tabWidget->addTab(charTab, "Персонаж");
    tabWidget->addTab(spellsTab, "Список Заклинаний");
    tabWidget->addTab(itemsTab, "Список Предметов");
    tabWidget->addTab(equipmentTab, "Список Оружия и доспехов");

    layout->addWidget(tabWidget);
}

void PlayerPage::startCharacterCreation()
{
    if (currentCharacter && !currentCharacter->name().trimmed().isEmpty()) {
        const auto answer = QMessageBox::question(
            this,
            "Перезаписать персонажа",
            QString("В кампании \"%1\" уже есть персонаж. Создать нового и заменить текущего?")
                .arg(currentCampaign.isEmpty() ? "без названия" : currentCampaign));

        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    bool ok = false;
    int chosenLevel = QInputDialog::getInt(
        this,
        "Уровень персонажа",
        "Выберите итоговый уровень персонажа (1-20):",
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
        "Имя персонажа",
        "Введите имя персонажа:",
        QLineEdit::Normal,
        currentCharacter ? currentCharacter->name() : QString(),
        &ok).trimmed();

    if (!ok) {
        return;
    }

    if (chosenName.isEmpty()) {
        QMessageBox::warning(this, "Имя персонажа", "Имя персонажа не должно быть пустым.");
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
    levelUpPreviousMaxHp = 0;
    levelUpPreviousFeatSlots = 0;
    levelUpSnapshot = QJsonObject();
    if (currentCharacter) {
        targetCharacterLevel = currentCharacter->level;
    }
}

void PlayerPage::resetClassSelection()
{
    allocatedClassLevels = 0;
    selectedClassLevels.clear();
    selectedClasses.clear();
    classSelectionOrder.clear();

    if (currentCharacter) {
        currentCharacter->setCharacterClass("");
        currentCharacter->classLevels.clear();
        currentCharacter->classHitDice.clear();
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
            parts << QString("%1 %2").arg(className).arg(classLevel);
        }
    }

    currentCharacter->setCharacterClass(parts.join(" / "));
}

void PlayerPage::prepareSelectedClassesFromCharacter()
{
    selectedClassLevels = currentCharacter ? currentCharacter->classLevels : QMap<QString, int>();
    classSelectionOrder = currentCharacter ? currentCharacter->classOrder : QStringList();

    if (classSelectionOrder.isEmpty()) {
        classSelectionOrder = selectedClassLevels.keys();
    }

    selectedClasses.clear();
    for (const QString &className : classSelectionOrder) {
        if (className.trimmed().isEmpty()) {
            continue;
        }
        selectedClasses.insert(className, classPage->getClassData(className));
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
    levelUpPreviousMaxHp = currentCharacter->maxHp;
    levelUpPreviousFeatSlots = totalFeatSlots(currentCharacter->classLevels);
    levelUpSnapshot = currentCharacter->toJson();
    targetCharacterLevel = qMin(20, currentCharacter->level + 1);
    prepareSelectedClassesFromCharacter();

    charStack->setCurrentIndex(2);
    classPage->showList();
    QMessageBox::information(
        this,
        QStringLiteral("Повышение уровня"),
        QStringLiteral("Выберите класс, в который будет вложен новый уровень."));
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
                lowered.contains(QStringLiteral("на 2")) &&
                lowered.contains(QStringLiteral("другой")) &&
                lowered.contains(QStringLiteral("на 1"))) {
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
    QPushButton *twoAbilitiesBtn = choiceBox.addButton(QStringLiteral("+1 к двум характеристикам"), QMessageBox::AcceptRole);
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
    const QRegularExpression amountRegex(QStringLiteral("на\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
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

        QPushButton *featBtn = choiceBox.addButton(QStringLiteral("Взять черту"), QMessageBox::AcceptRole);
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

    if (spellSelections.isEmpty()) {
        currentCharacter->spells = preservedRaceSpells;
        currentCharacter->spellbook = preservedRaceSpellbook;
        return;
    }

    const bool singleCastingClass = spellSelections.size() == 1;
    const QList<Spell> previousPreparedSpells = currentCharacter->spells;
    const QList<Spell> previousSpellbook = currentCharacter->spellbook;
    QList<Spell> rebuiltPreparedSpells = preservedRaceSpells;
    QList<Spell> rebuiltSpellbook = preservedRaceSpellbook;

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

    currentCharacter->spells = rebuiltPreparedSpells;
    currentCharacter->spellbook = rebuiltSpellbook;
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

void PlayerPage::onRaceChosen(const Race &race)
{
    if (!currentCharacter) {
        return;
    }

    resetClassSelection();
    applyBaseAbilityScores();

    currentCharacter->setRace(race.name);
    currentCharacter->size = race.size;
    currentCharacter->speed = race.speed;
    currentCharacter->flyingSpeed = race.flyingSpeed;

    QStringList resolvedRaceLanguages;
    if (!resolveChosenLanguages(raceLanguageEntriesForSelection(race), race.name, {}, &resolvedRaceLanguages)) {
        return;
    }

    if (!applyRaceAbilityBonuses(race)) {
        return;
    }

    currentCharacter->languages = resolvedRaceLanguages;

    if (!chooseRaceGrantedSpells(race)) {
        return;
    }

    currentCharacter->traits = race.traits;
    const QStringList baseSkills = currentCharacter->skillProficiencies;
    const QStringList baseTools = currentCharacter->toolProficiencies;
    const QStringList baseArmor = currentCharacter->armorProficiencies;
    const QStringList baseWeapons = currentCharacter->weaponProficiencies;
    applyRaceDerivedBenefits(race);

    if (!applyRaceChoiceBenefits(this, race, currentCharacter, baseSkills, baseTools, baseArmor, baseWeapons)) {
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
    currentCharacter->classHitDice.clear();
    currentCharacter->savingThrowProficiencies.clear();
    currentCharacter->armorProficiencies.clear();
    currentCharacter->weaponProficiencies.clear();

    const int projectedLevel = qMax(1, sumLevels(selectedClassLevels) > 0 ? sumLevels(selectedClassLevels) : targetCharacterLevel);
    const QMap<QString, QString> characterRaceTraits = normalizedRaceTraits(currentCharacter->traits);
    QStringList armorProficiencies = racialArmorProficiencies(characterRaceTraits, projectedLevel);
    QStringList weaponProficiencies = racialWeaponProficiencies(characterRaceTraits, projectedLevel);
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
    }

    if (firstClass) {
        currentCharacter->hitDie = 0;
    }

    currentCharacter->armorProficiencies = uniqueStrings(armorProficiencies);
    currentCharacter->weaponProficiencies = uniqueStrings(weaponProficiencies);

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

    selectedClassLevels[cls.name] += classLevel;
    selectedClasses[cls.name] = cls;
    if (!classSelectionOrder.contains(cls.name)) {
        classSelectionOrder << cls.name;
    }

    synchronizeCharacterFromClasses(!levelUpInProgress);

    qDebug() << "Character Class Selected:" << cls.name << "Level:" << classLevel;

    remaining = remainingLevelsToAllocate();
    if (remaining > 0) {
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
            selectedClassLevels[cls.name] += remaining;
            synchronizeCharacterFromClasses(!levelUpInProgress);
        }
    }

    completeCharacterCreation();
}



