#include "bestiarywidget.h"
#include "databasemanager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QListWidgetItem>

BestiaryWidget::BestiaryWidget(QWidget *parent) : QWidget(parent)
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    // Left Side: List and Search
    QWidget *leftContainer = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftContainer);
    
    searchBar = new QLineEdit();
    searchBar->setPlaceholderText("Поиск существа...");
    connect(searchBar, &QLineEdit::textChanged, this, &BestiaryWidget::filterCreatures);
    
    creatureList = new QListWidget();
    connect(creatureList, &QListWidget::itemClicked, this, &BestiaryWidget::onCreatureSelected);
    
    leftLayout->addWidget(searchBar);
    leftLayout->addWidget(creatureList);
    leftContainer->setMaximumWidth(300);
    
    mainLayout->addWidget(leftContainer);
    
    // Right Side: Details (Stat Block)
    QGroupBox *detailsGroup = new QGroupBox("Параметры существа");
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsGroup);
    
    detailsText = new QTextEdit();
    detailsText->setReadOnly(true);
    
    detailsLayout->addWidget(detailsText);
    mainLayout->addWidget(detailsGroup, 1);
    
    loadCreatures();
}

void BestiaryWidget::loadCreatures()
{
    allCreatures = DatabaseManager::instance().getAllCreatures();
    
    // Sort
    std::sort(allCreatures.begin(), allCreatures.end(), [](const Creature& a, const Creature& b){
        return a.name < b.name; 
    });
    
    filterCreatures();
}

void BestiaryWidget::filterCreatures()
{
    creatureList->clear();
    QString search = searchBar->text().toLower();
    
    for (const auto& c : allCreatures) {
        if (c.name.toLower().contains(search) || c.nameEng.toLower().contains(search)) {
            QListWidgetItem *item = new QListWidgetItem(c.name);
            item->setData(Qt::UserRole, c.id);
            creatureList->addItem(item);
        }
    }
}

void BestiaryWidget::onCreatureSelected(QListWidgetItem *item)
{
    int id = item->data(Qt::UserRole).toInt();
    
    // Find creature
    Creature selected;
    for (const auto& c : allCreatures) {
        if (c.id == id) {
            selected = c;
            break;
        }
    }
    
    detailsText->setHtml(generateHtml(selected));
}

QString BestiaryWidget::formatModifier(int score)
{
    int mod = (score - 10) / 2;
    if (mod >= 0) return QString("+%1").arg(mod);
    return QString("%1").arg(mod);
}

QString BestiaryWidget::generateHtml(const Creature& c)
{
    QString html = "<div style='font-family: Arial; padding: 10px;'>";
    
    // Header
    html += QString("<h1 style='color: #E60000; margin-bottom: 0;'>%1</h1>").arg(c.name);
    html += QString("<h3 style='color: #888; margin-top: 0;'>%1</h3>").arg(c.nameEng);
    html += QString("<p><i>%1</i></p>").arg(c.type);
    html += "<hr style='border: 1px solid #E60000;'>";
    
    // Base Stats
    html += QString("<p><b>Класс Доспеха:</b> %1</p>").arg(c.ac);
    html += QString("<p><b>Хиты:</b> %1</p>").arg(c.hp);
    html += QString("<p><b>Скорость:</b> %1</p>").arg(c.speed);
    html += "<hr style='border: 1px solid #E60000;'>";
    
    // Ability Scores Table
    html += "<table style='width: 100%; text-align: center;'>";
    html += "<tr><th>СИЛ</th><th>ЛОВ</th><th>ТЕЛ</th><th>ИНТ</th><th>МДР</th><th>ХАР</th></tr>";
    html += QString("<tr><td>%1 (%2)</td><td>%3 (%4)</td><td>%5 (%6)</td><td>%7 (%8)</td><td>%9 (%10)</td><td>%11 (%12)</td></tr>")
            .arg(c.str).arg(formatModifier(c.str))
            .arg(c.dex).arg(formatModifier(c.dex))
            .arg(c.con).arg(formatModifier(c.con))
            .arg(c.inte).arg(formatModifier(c.inte))
            .arg(c.wis).arg(formatModifier(c.wis))
            .arg(c.cha).arg(formatModifier(c.cha));
    html += "</table>";
    html += "<hr style='border: 1px solid #E60000;'>";
    
    // Info
    if (!c.senses.isEmpty()) html += QString("<p><b>Чувства:</b> %1</p>").arg(c.senses);
    if (!c.languages.isEmpty()) html += QString("<p><b>Языки:</b> %1</p>").arg(c.languages);
    if (!c.challenge.isEmpty()) html += QString("<p><b>Опасность:</b> %1</p>").arg(c.challenge);
    html += "<hr style='border: 1px solid #E60000;'>";
    
    // Traits
    if (!c.traits.isEmpty()) {
        for (const auto& t : c.traits) {
            html += QString("<p><b>%1.</b> %2</p>").arg(t.name, t.text);
        }
    }
    
    // Actions
    if (!c.actions.isEmpty()) {
        html += "<h2 style='color: #E60000; border-bottom: 1px solid #E60000;'>Действия</h2>";
        for (const auto& a : c.actions) {
            html += QString("<p><b>%1.</b> %2</p>").arg(a.name, a.text);
        }
    }
    
    // Legendary Actions
    if (!c.legendaryActions.isEmpty()) {
        html += "<h2 style='color: #E60000; border-bottom: 1px solid #E60000;'>Легендарные действия</h2>";
        html += "<p>Существо может совершить определенное количество легендарных действий, выбирая из представленных ниже вариантов. Ограничение: 1 вариант за раз и только в конце хода другого существа. Существо восстанавливает потраченные легендарные действия в начале своего хода.</p>";
        for (const auto& l : c.legendaryActions) {
            html += QString("<p><b>%1.</b> %2</p>").arg(l.name, l.text);
        }
    }
    
    // Description (Lore)
    if (!c.description.isEmpty()) {
        html += "<h3 style='color: #888;'>Описание</h3>";
        html += QString("<div>%1</div>").arg(c.description);
    }
    
    html += "<p style='color: #666; font-size: 0.8em;'><i>Источник: " + c.source + "</i></p>";
    
    html += "</div>";
    return html;
}
