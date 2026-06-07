#include "bestiarywidget.h"

#include "databasemanager.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {

struct DifficultyThreshold {
    int easy = 0;
    int medium = 0;
    int hard = 0;
    int deadly = 0;
};

struct CrXpEntry {
    const char *cr = "";
    int xp = 0;
};

const DifficultyThreshold kThresholdsByLevel[21] = {
    {0, 0, 0, 0},
    {25, 50, 75, 100},
    {50, 100, 150, 200},
    {75, 150, 225, 400},
    {125, 250, 375, 500},
    {250, 500, 750, 1100},
    {300, 600, 900, 1400},
    {350, 750, 1100, 1700},
    {450, 900, 1400, 2100},
    {550, 1100, 1600, 2400},
    {600, 1200, 1900, 2800},
    {800, 1600, 2400, 3600},
    {1000, 2000, 3000, 4500},
    {1100, 2200, 3400, 5100},
    {1250, 2500, 3800, 5700},
    {1400, 2800, 4300, 6400},
    {1600, 3200, 4800, 7200},
    {2000, 3900, 5900, 8800},
    {2100, 4200, 6300, 9500},
    {2400, 4900, 7300, 10900},
    {2800, 5700, 8500, 12700}
};

const CrXpEntry kCrXpTable[] = {
    {"0", 10},
    {"1/8", 25},
    {"1/4", 50},
    {"1/2", 100},
    {"1", 200},
    {"2", 450},
    {"3", 700},
    {"4", 1100},
    {"5", 1800},
    {"6", 2300},
    {"7", 2900},
    {"8", 3900},
    {"9", 5000},
    {"10", 5900},
    {"11", 7200},
    {"12", 8400},
    {"13", 10000},
    {"14", 11500},
    {"15", 13000},
    {"16", 15000},
    {"17", 18000},
    {"18", 20000},
    {"19", 22000},
    {"20", 25000},
    {"21", 33000},
    {"22", 41000},
    {"23", 50000},
    {"24", 62000},
    {"25", 75000},
    {"26", 90000},
    {"27", 105000},
    {"28", 120000},
    {"29", 135000},
    {"30", 155000}
};

} // namespace

BestiaryWidget::BestiaryWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("BestiaryWidget"));

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    auto *leftContainer = new QWidget(splitter);
    leftContainer->setMinimumWidth(260);
    leftContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto *leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto *filterGroup = new QGroupBox(QStringLiteral("Фильтры"), leftContainer);
    auto *filterForm = new QFormLayout(filterGroup);

    searchBar = new QLineEdit(filterGroup);
    searchBar->setPlaceholderText(QStringLiteral("Поиск существа..."));

    challengeFilter = new QComboBox(filterGroup);
    challengeFilter->addItem(QStringLiteral("Все КУ"), QString());

    challengeGuideBtn = new QPushButton(QStringLiteral("Памятка по КУ (DnD 5e)"), filterGroup);
    challengeGuideBtn->setMinimumHeight(34);
    challengeGuideBtn->setMinimumWidth(250);
    challengeGuideBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    filterForm->addRow(QStringLiteral("Название:"), searchBar);
    filterForm->addRow(QStringLiteral("Опасность (КУ):"), challengeFilter);
    filterForm->addRow(QString(), challengeGuideBtn);

    leftLayout->addWidget(filterGroup);

    creatureList = new QListWidget(leftContainer);
    leftLayout->addWidget(creatureList, 1);

    connect(searchBar, &QLineEdit::textChanged, this, &BestiaryWidget::filterCreatures);
    connect(challengeFilter, &QComboBox::currentTextChanged, this, &BestiaryWidget::filterCreatures);
    connect(creatureList, &QListWidget::itemClicked, this, &BestiaryWidget::onCreatureSelected);
    connect(challengeGuideBtn, &QPushButton::clicked, this, &BestiaryWidget::showChallengeGuideDialog);

    auto *rightContainer = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    auto *detailsGroup = new QGroupBox(QStringLiteral("Параметры существа"), rightContainer);
    auto *detailsLayout = new QVBoxLayout(detailsGroup);

    detailsText = new QTextEdit(detailsGroup);
    detailsText->setReadOnly(true);
    detailsLayout->addWidget(detailsText, 1);

    rightLayout->addWidget(detailsGroup, 1);

    splitter->addWidget(leftContainer);
    splitter->addWidget(rightContainer);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({320, 980});

    mainLayout->addWidget(splitter, 1);

    loadCreatures();
}

void BestiaryWidget::loadCreatures()
{
    allCreatures = DatabaseManager::instance().getAllCreatures();
    std::sort(allCreatures.begin(), allCreatures.end(), [](const Creature &a, const Creature &b) {
        return a.name < b.name;
    });

    populateChallengeFilter();
    filterCreatures();
}

void BestiaryWidget::populateChallengeFilter()
{
    if (!challengeFilter) {
        return;
    }

    QSignalBlocker blocker(challengeFilter);
    const QString previouslySelected = challengeFilter->currentData().toString();

    challengeFilter->clear();
    challengeFilter->addItem(QStringLiteral("Все КУ"), QString());

    QStringList tokens;
    for (const Creature &creature : std::as_const(allCreatures)) {
        const QString token = normalizeChallengeToken(creature.challenge);
        if (!token.isEmpty() && !tokens.contains(token)) {
            tokens << token;
        }
    }

    std::sort(tokens.begin(), tokens.end(), [this](const QString &a, const QString &b) {
        const double av = challengeValue(a);
        const double bv = challengeValue(b);
        if (!qFuzzyCompare(av + 1.0, bv + 1.0)) {
            return av < bv;
        }
        return a < b;
    });

    for (const QString &token : std::as_const(tokens)) {
        challengeFilter->addItem(QStringLiteral("КУ %1").arg(token), token);
    }

    const int restoredIndex = challengeFilter->findData(previouslySelected);
    challengeFilter->setCurrentIndex(restoredIndex >= 0 ? restoredIndex : 0);
}

void BestiaryWidget::filterCreatures()
{
    creatureList->clear();
    const QString search = searchBar ? searchBar->text().trimmed().toLower() : QString();

    for (const Creature &creature : std::as_const(allCreatures)) {
        const bool matchesSearch = search.isEmpty()
            || creature.name.toLower().contains(search)
            || creature.nameEng.toLower().contains(search);
        if (!matchesSearch || !matchesSelectedChallenge(creature)) {
            continue;
        }

        auto *item = new QListWidgetItem(creature.name, creatureList);
        item->setData(Qt::UserRole, creature.id);
    }
}

bool BestiaryWidget::matchesSelectedChallenge(const Creature &creature) const
{
    if (!challengeFilter) {
        return true;
    }

    const QString selected = challengeFilter->currentData().toString();
    if (selected.isEmpty()) {
        return true;
    }

    return normalizeChallengeToken(creature.challenge) == selected;
}

QString BestiaryWidget::normalizeChallengeToken(const QString &challenge) const
{
    const QString text = challenge.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    static const QRegularExpression re(QStringLiteral("(\\d+\\s*/\\s*\\d+|\\d+)"));
    const QRegularExpressionMatch match = re.match(text);
    if (match.hasMatch()) {
        QString token = match.captured(1);
        token.remove(QLatin1Char(' '));
        return token;
    }

    return {};
}

double BestiaryWidget::challengeValue(const QString &token) const
{
    if (token.contains(QLatin1Char('/'))) {
        const QStringList parts = token.split(QLatin1Char('/'));
        if (parts.size() == 2) {
            bool okNum = false;
            bool okDen = false;
            const double num = parts.at(0).toDouble(&okNum);
            const double den = parts.at(1).toDouble(&okDen);
            if (okNum && okDen && den > 0.0) {
                return num / den;
            }
        }
    }

    bool ok = false;
    const double value = token.toDouble(&ok);
    return ok ? value : 9999.0;
}

void BestiaryWidget::onCreatureSelected(QListWidgetItem *item)
{
    if (!item) {
        return;
    }

    const int id = item->data(Qt::UserRole).toInt();
    Creature selected;
    for (const Creature &creature : std::as_const(allCreatures)) {
        if (creature.id == id) {
            selected = creature;
            break;
        }
    }

    detailsText->setHtml(generateHtml(selected));
}

QString BestiaryWidget::formatModifier(int score)
{
    const int mod = static_cast<int>(std::floor((score - 10) / 2.0));
    return mod >= 0 ? QStringLiteral("+%1").arg(mod) : QString::number(mod);
}

QString BestiaryWidget::generateHtml(const Creature &c)
{
    QString html = QStringLiteral("<div style='font-family: Segoe UI; padding: 10px;'>");

    html += QStringLiteral("<h1 style='color: #9d3a34; margin-bottom: 0;'>%1</h1>").arg(c.name);
    html += QStringLiteral("<h3 style='color: #5e503f; margin-top: 0;'>%1</h3>").arg(c.nameEng);
    html += QStringLiteral("<p><i>%1</i></p>").arg(c.type);
    html += QStringLiteral("<hr style='border: 1px solid #9d3a34;'>");

    html += QStringLiteral("<p><b>Класс Доспеха:</b> %1</p>").arg(c.ac);
    html += QStringLiteral("<p><b>Хиты:</b> %1</p>").arg(c.hp);
    html += QStringLiteral("<p><b>Скорость:</b> %1</p>").arg(c.speed);
    html += QStringLiteral("<hr style='border: 1px solid #9d3a34;'>");

    html += QStringLiteral("<table style='width: 100%; text-align: center;'>");
    html += QStringLiteral("<tr><th>СИЛ</th><th>ЛОВ</th><th>ТЕЛ</th><th>ИНТ</th><th>МДР</th><th>ХАР</th></tr>");
    html += QStringLiteral("<tr><td>%1 (%2)</td><td>%3 (%4)</td><td>%5 (%6)</td><td>%7 (%8)</td><td>%9 (%10)</td><td>%11 (%12)</td></tr>")
                .arg(c.str).arg(formatModifier(c.str))
                .arg(c.dex).arg(formatModifier(c.dex))
                .arg(c.con).arg(formatModifier(c.con))
                .arg(c.inte).arg(formatModifier(c.inte))
                .arg(c.wis).arg(formatModifier(c.wis))
                .arg(c.cha).arg(formatModifier(c.cha));
    html += QStringLiteral("</table>");
    html += QStringLiteral("<hr style='border: 1px solid #9d3a34;'>");

    if (!c.senses.isEmpty()) {
        html += QStringLiteral("<p><b>Чувства:</b> %1</p>").arg(c.senses);
    }
    if (!c.languages.isEmpty()) {
        html += QStringLiteral("<p><b>Языки:</b> %1</p>").arg(c.languages);
    }
    if (!c.challenge.isEmpty()) {
        html += QStringLiteral("<p><b>Опасность:</b> %1</p>").arg(c.challenge);
    }
    html += QStringLiteral("<hr style='border: 1px solid #9d3a34;'>");

    if (!c.traits.isEmpty()) {
        for (const CreatureAction &trait : c.traits) {
            html += QStringLiteral("<p><b>%1.</b> %2</p>").arg(trait.name, trait.text);
        }
    }

    if (!c.actions.isEmpty()) {
        html += QStringLiteral("<h2 style='color: #9d3a34; border-bottom: 1px solid #9d3a34;'>Действия</h2>");
        for (const CreatureAction &action : c.actions) {
            html += QStringLiteral("<p><b>%1.</b> %2</p>").arg(action.name, action.text);
        }
    }

    if (!c.legendaryActions.isEmpty()) {
        html += QStringLiteral("<h2 style='color: #9d3a34; border-bottom: 1px solid #9d3a34;'>Легендарные действия</h2>");
        html += QStringLiteral("<p>Существо может совершить ограниченное число легендарных действий, выбирая варианты ниже.</p>");
        for (const CreatureAction &legendary : c.legendaryActions) {
            html += QStringLiteral("<p><b>%1.</b> %2</p>").arg(legendary.name, legendary.text);
        }
    }

    if (!c.description.isEmpty()) {
        html += QStringLiteral("<h3 style='color: #5e503f;'>Описание</h3>");
        html += QStringLiteral("<div>%1</div>").arg(c.description);
    }

    if (!c.source.isEmpty()) {
        html += QStringLiteral("<p style='color: #5e503f; font-size: 0.9em;'><i>Источник: %1</i></p>").arg(c.source);
    }

    html += QStringLiteral("</div>");
    return html;
}

QString BestiaryWidget::buildDynamicMemoText(int partySize, int level) const
{
    const DifficultyThreshold threshold = kThresholdsByLevel[level];

    const int easyXp = threshold.easy * partySize;
    const int mediumXp = threshold.medium * partySize;
    const int hardXp = threshold.hard * partySize;
    const int deadlyXp = threshold.deadly * partySize;

    return QStringLiteral(
               "Группа: %1 персонажа(ей), уровень %2.\n"
               "Легко: %3 XP (примерно 1 монстр КУ %4)\n"
               "Средне: %5 XP (примерно 1 монстр КУ %6)\n"
               "Тяжело: %7 XP (примерно 1 монстр КУ %8)\n"
               "Смертельно: %9 XP (примерно 1 монстр КУ %10)")
        .arg(partySize)
        .arg(level)
        .arg(easyXp)
        .arg(challengeForThresholdXp(easyXp))
        .arg(mediumXp)
        .arg(challengeForThresholdXp(mediumXp))
        .arg(hardXp)
        .arg(challengeForThresholdXp(hardXp))
        .arg(deadlyXp)
        .arg(challengeForThresholdXp(deadlyXp));
}

QString BestiaryWidget::challengeForThresholdXp(int xp) const
{
    QString chosen = QStringLiteral("0");
    for (const CrXpEntry &entry : kCrXpTable) {
        if (entry.xp <= xp) {
            chosen = QString::fromLatin1(entry.cr);
        } else {
            break;
        }
    }
    return chosen;
}

QString BestiaryWidget::buildChallengeGuideHtml() const
{
    return QStringLiteral(
        "<div style='font-family: Segoe UI;'>"
        "<b>Как читать КУ по правилам DnD 5e:</b><br/>"
        "• Порог сложности считается через XP (лёгко/средне/тяжело/смертельно).<br/>"
        "• Таблица с XP выше даёт оценку для <b>1 монстра</b> без множителей.<br/>"
        "• Для нескольких монстров применяйте множитель сложности (DMG):<br/>"
        "&nbsp;&nbsp;2 монстра ×1.5, 3–6 ×2, 7–10 ×2.5, 11–14 ×3, 15+ ×4.<br/>"
        "• Базовое ориентировочное правило: для группы из 4 персонажей один монстр КУ, "
        "близкого к уровню группы, обычно даёт «средний» бой.<br/>"
        "<span style='color:#5e503f;'>Подбор боя всегда корректируйте под ресурсы группы, "
        "состав классов и тактику.</span>"
        "</div>");
}

void BestiaryWidget::showChallengeGuideDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Памятка по КУ (DnD 5e)"));
    dialog.resize(760, 520);

    auto *layout = new QVBoxLayout(&dialog);

    auto *controls = new QHBoxLayout();
    controls->addWidget(new QLabel(QStringLiteral("Размер группы:"), &dialog));

    auto *partySizeSpin = new QSpinBox(&dialog);
    partySizeSpin->setRange(1, 10);
    partySizeSpin->setValue(guidePartySize);
    controls->addWidget(partySizeSpin);

    controls->addSpacing(12);
    controls->addWidget(new QLabel(QStringLiteral("Уровень персонажей:"), &dialog));

    auto *partyLevelSpin = new QSpinBox(&dialog);
    partyLevelSpin->setRange(1, 20);
    partyLevelSpin->setValue(guidePartyLevel);
    controls->addWidget(partyLevelSpin);

    controls->addStretch(1);
    layout->addLayout(controls);

    auto *dynamicMemoLabel = new QLabel(&dialog);
    dynamicMemoLabel->setWordWrap(true);
    dynamicMemoLabel->setProperty("role", QStringLiteral("muted"));
    dynamicMemoLabel->setText(buildDynamicMemoText(guidePartySize, guidePartyLevel));
    layout->addWidget(dynamicMemoLabel);

    auto *challengeGuideText = new QTextEdit(&dialog);
    challengeGuideText->setReadOnly(true);
    challengeGuideText->setHtml(buildChallengeGuideHtml());
    layout->addWidget(challengeGuideText, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    auto updateGuideText = [this, dynamicMemoLabel, partySizeSpin, partyLevelSpin]() {
        guidePartySize = partySizeSpin->value();
        guidePartyLevel = partyLevelSpin->value();
        dynamicMemoLabel->setText(buildDynamicMemoText(guidePartySize, guidePartyLevel));
    };

    connect(partySizeSpin, qOverload<int>(&QSpinBox::valueChanged), this, [updateGuideText]() {
        updateGuideText();
    });
    connect(partyLevelSpin, qOverload<int>(&QSpinBox::valueChanged), this, [updateGuideText]() {
        updateGuideText();
    });

    dialog.exec();
}
