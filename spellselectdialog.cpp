#include "spellselectdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QGroupBox>
#include <QDebug>
#include "databasemanager.h"

SpellSelectDialog::SpellSelectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Выбор заклинаний для НПС");
    resize(1000, 600);
    
    loadAllSpells();
    setupUi();
    setupFilters();
    
    // Initial filter
    filterSpells();
}

void SpellSelectDialog::loadAllSpells()
{
    DatabaseManager &dbManager = DatabaseManager::instance();
    m_allSpells = dbManager.getAllSpells();
}

void SpellSelectDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Top filter panel
    QGroupBox *filterGroup = new QGroupBox("Фильтры", this);
    QVBoxLayout *filterLayout = new QVBoxLayout(filterGroup);
    
    // Search bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel("Поиск:"));
    searchBar = new QLineEdit();
    searchBar->setPlaceholderText("Название заклинания...");
    connect(searchBar, &QLineEdit::textChanged, this, &SpellSelectDialog::filterSpells);
    searchLayout->addWidget(searchBar);
    filterLayout->addLayout(searchLayout);
    
    // Filter row 1
    QHBoxLayout *filterRow1 = new QHBoxLayout();
    
    filterRow1->addWidget(new QLabel("Класс:"));
    classFilter = new QComboBox();
    classFilter->addItem("Все классы");
    connect(classFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpellSelectDialog::filterSpells);
    filterRow1->addWidget(classFilter);
    
    filterRow1->addWidget(new QLabel("Уровень:"));
    levelFilter = new QComboBox();
    levelFilter->addItem("Все уровни");
    for (int i = 0; i <= 9; ++i) {
        levelFilter->addItem(QString::number(i));
    }
    connect(levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpellSelectDialog::filterSpells);
    filterRow1->addWidget(levelFilter);
    
    filterRow1->addWidget(new QLabel("Школа:"));
    schoolFilter = new QComboBox();
    schoolFilter->addItem("Все школы");
    connect(schoolFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpellSelectDialog::filterSpells);
    filterRow1->addWidget(schoolFilter);
    
    filterRow1->addStretch();
    filterLayout->addLayout(filterRow1);
    
    // Filter row 2
    QHBoxLayout *filterRow2 = new QHBoxLayout();
    
    filterRow2->addWidget(new QLabel("Время касания:"));
    castingTimeFilter = new QComboBox();
    castingTimeFilter->addItem("Все время");
    connect(castingTimeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpellSelectDialog::filterSpells);
    filterRow2->addWidget(castingTimeFilter);
    
    checkV = new QCheckBox("Вербальные");
    checkS = new QCheckBox("Соматические");
    checkM = new QCheckBox("Материальные");
    checkConcentration = new QCheckBox("Концентрация");
    checkRitual = new QCheckBox("Ритуал");
    
    connect(checkV, &QCheckBox::checkStateChanged, this, &SpellSelectDialog::filterSpells);
    connect(checkS, &QCheckBox::checkStateChanged, this, &SpellSelectDialog::filterSpells);
    connect(checkM, &QCheckBox::checkStateChanged, this, &SpellSelectDialog::filterSpells);
    connect(checkConcentration, &QCheckBox::checkStateChanged, this, &SpellSelectDialog::filterSpells);
    connect(checkRitual, &QCheckBox::checkStateChanged, this, &SpellSelectDialog::filterSpells);
    
    filterRow2->addWidget(checkV);
    filterRow2->addWidget(checkS);
    filterRow2->addWidget(checkM);
    filterRow2->addWidget(checkConcentration);
    filterRow2->addWidget(checkRitual);
    filterRow2->addStretch();
    filterLayout->addLayout(filterRow2);
    
    mainLayout->addWidget(filterGroup);
    
    // Main content - Splitter
    QHBoxLayout *contentLayout = new QHBoxLayout();
    
    // Left side - Available spells
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    
    leftLayout->addWidget(new QLabel("Доступные заклинания:"));
    availableSpellList = new QListWidget();
    connect(availableSpellList, &QListWidget::itemSelectionChanged, this, &SpellSelectDialog::onSpellSelected);
    connect(availableSpellList, &QListWidget::itemDoubleClicked, this, &SpellSelectDialog::onSpellDoubleClicked);
    leftLayout->addWidget(availableSpellList);
    
    nameLabel = new QLabel();
    nameLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    leftLayout->addWidget(nameLabel);
    
    infoLabel = new QLabel();
    infoLabel->setStyleSheet("color: gray;");
    leftLayout->addWidget(infoLabel);
    
    descriptionText = new QTextEdit();
    descriptionText->setReadOnly(true);
    descriptionText->setMaximumHeight(150);
    leftLayout->addWidget(descriptionText);
    
    // Center - Add/Remove buttons
    QVBoxLayout *centerLayout = new QVBoxLayout();
    centerLayout->addStretch();
    
    addBtn = new QPushButton("➜ Добавить ➜");
    addBtn->setMinimumHeight(40);
    connect(addBtn, &QPushButton::clicked, this, &SpellSelectDialog::addSelectedSpell);
    centerLayout->addWidget(addBtn);
    
    removeBtn = new QPushButton("✕ Удалить");
    removeBtn->setMinimumHeight(40);
    connect(removeBtn, &QPushButton::clicked, this, &SpellSelectDialog::removeSelectedSpell);
    centerLayout->addWidget(removeBtn);
    
    centerLayout->addStretch();
    
    // Right side - Selected spells
    QWidget *rightWidget = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    
    rightLayout->addWidget(new QLabel("Выбранные заклинания:"));
    selectedSpellList = new QListWidget();
    rightLayout->addWidget(selectedSpellList);
    
    // Content splitter
    contentLayout->addWidget(leftWidget, 1);
    contentLayout->addLayout(centerLayout);
    contentLayout->addWidget(rightWidget, 1);
    
    mainLayout->addLayout(contentLayout);
    
    // Bottom buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton *okBtn = new QPushButton("OK");
    QPushButton *cancelBtn = new QPushButton("Отмена");
    
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    
    mainLayout->addLayout(buttonLayout);
}

void SpellSelectDialog::setupFilters()
{
    // Populate class filter
    QStringList classes = {"Бард", "Клирик", "Друид", "Паладин", "Рейнджер", "Чародей", "Чернокнижник", "Волшебник"};
    for (const QString &cls : classes) {
        classFilter->addItem(cls);
    }
    
    // Populate school filter
    QStringList schools = {"Абъюрация", "Конъюрация", "Дивинация", "Очарование", 
                          "Эвокация", "Иллюзия", "Некромантия", "Трансмутация"};
    for (const QString &school : schools) {
        schoolFilter->addItem(school);
    }
    
    // Populate casting time filter
    QStringList castingTimes = {"Действие", "Бонусное действие", "Реакция", "1 минута", "10 минут", "1 час"};
    for (const QString &time : castingTimes) {
        castingTimeFilter->addItem(time);
    }
}

void SpellSelectDialog::filterSpells()
{
    QString searchText = searchBar->text().toLower();
    QString classText = classFilter->currentText();
    QString levelText = levelFilter->currentText();
    QString schoolText = schoolFilter->currentText();
    QString castingTimeText = castingTimeFilter->currentText();
    
    availableSpellList->clear();
    m_filteredSpells.clear();
    
    for (const Spell &spell : m_allSpells) {
        // Search filter
        if (!searchText.isEmpty() && !spell.name.toLower().contains(searchText)) {
            continue;
        }
        
        // Level filter
        if (levelText != "Все уровни" && spell.level != levelText.toInt()) {
            continue;
        }
        
        // School filter
        if (schoolText != "Все школы" && !spell.school.contains(schoolText, Qt::CaseInsensitive)) {
            continue;
        }
        
        // Component filters
        if (checkV->isChecked() && !spell.components.contains("V", Qt::CaseInsensitive)) {
            continue;
        }
        if (checkS->isChecked() && !spell.components.contains("S", Qt::CaseInsensitive)) {
            continue;
        }
        if (checkM->isChecked() && !spell.components.contains("M", Qt::CaseInsensitive)) {
            continue;
        }
        if (checkConcentration->isChecked() && !spell.description.contains("concentration", Qt::CaseInsensitive)) {
            continue;
        }
        if (checkRitual->isChecked() && !spell.description.contains("ritual", Qt::CaseInsensitive)) {
            continue;
        }
        
        m_filteredSpells.append(spell);
        QListWidgetItem *item = new QListWidgetItem(spell.name);
        item->setData(Qt::UserRole, spell.name);
        availableSpellList->addItem(item);
    }
}

void SpellSelectDialog::onSpellSelected()
{
    QListWidgetItem *item = availableSpellList->currentItem();
    if (!item) {
        return;
    }
    
    QString spellName = item->data(Qt::UserRole).toString();
    
    for (const Spell &spell : m_filteredSpells) {
        if (spell.name == spellName) {
            nameLabel->setText(spell.name);
            
            QString info = QString("Уровень: %1 | Школа: %2 | Компоненты: %3")
                .arg(spell.level)
                .arg(spell.school)
                .arg(spell.components);
            infoLabel->setText(info);
            
            descriptionText->setPlainText(spell.description);
            break;
        }
    }
}

void SpellSelectDialog::onSpellDoubleClicked(QListWidgetItem *)
{
    addSelectedSpell();
}

void SpellSelectDialog::addSelectedSpell()
{
    QListWidgetItem *item = availableSpellList->currentItem();
    if (!item) return;
    
    QString spellName = item->data(Qt::UserRole).toString();
    
    // Check if already selected
    for (const Spell &selected : m_selectedSpells) {
        if (selected.name == spellName) {
            return; // Already selected
        }
    }
    
    // Find and add spell
    for (const Spell &spell : m_filteredSpells) {
        if (spell.name == spellName) {
            m_selectedSpells.append(spell);
            break;
        }
    }
    
    updateSelectedList();
}

void SpellSelectDialog::removeSelectedSpell()
{
    QListWidgetItem *item = selectedSpellList->currentItem();
    if (!item) return;
    
    QString spellName = item->data(Qt::UserRole).toString();
    
    for (int i = 0; i < m_selectedSpells.size(); ++i) {
        if (m_selectedSpells[i].name == spellName) {
            m_selectedSpells.removeAt(i);
            break;
        }
    }
    
    updateSelectedList();
}

void SpellSelectDialog::updateSelectedList()
{
    selectedSpellList->clear();
    
    for (const Spell &spell : m_selectedSpells) {
        QListWidgetItem *item = new QListWidgetItem(QString("%1 (уровень %2)").arg(spell.name).arg(spell.level));
        item->setData(Qt::UserRole, spell.name);
        selectedSpellList->addItem(item);
    }
}

QList<Spell> SpellSelectDialog::selectedSpells() const
{
    return m_selectedSpells;
}
