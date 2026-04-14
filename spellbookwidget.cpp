#include "spellbookwidget.h"
#include "databasemanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QGridLayout>

SpellBookWidget::SpellBookWidget(QWidget *parent) : QWidget(parent)
{
    setupUi();
    loadAllSpells();
    filterSpells(); // Initial population
}

void SpellBookWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- Search & Filters ---
    QGroupBox *filterGroup = new QGroupBox("Фильтры", this);
    QGridLayout *filterLayout = new QGridLayout(filterGroup);
    filterLayout->setContentsMargins(5, 5, 5, 5);

    // Row 0: Search
    searchBar = new QLineEdit(this);
    searchBar->setPlaceholderText("Название заклинания...");
    connect(searchBar, &QLineEdit::textChanged, this, &SpellBookWidget::filterSpells);
    filterLayout->addWidget(new QLabel("Поиск:"), 0, 0);
    filterLayout->addWidget(searchBar, 0, 1, 1, 5); // Span multiple cols

    // Row 1: Class, Level, School
    classFilter = new QComboBox(this);
    classFilter->addItems({"Все классы", "Бард", "Жрец", "Друид", "Паладин", "Следопыт", "Чародей", "Колдун", "Волшебник", "Изобретатель", "Псионик"});
    connect(classFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpellBookWidget::filterSpells);
    
    levelFilter = new QComboBox(this);
    levelFilter->addItem("Все уровни");
    levelFilter->addItem("Заговор (0)");
    for(int i=1; i<=9; ++i) levelFilter->addItem(QString("Уровень %1").arg(i));
    connect(levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpellBookWidget::filterSpells);

    schoolFilter = new QComboBox(this); // Can interpret as School or Damage filters
    schoolFilter->addItems({"Все школы", "Воплощение", "Вызов", "Иллюзия", "Некромантия", "Ограждение", "Очарование", "Преобразование", "Прорицание"});
    connect(schoolFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpellBookWidget::filterSpells);
    
    filterLayout->addWidget(new QLabel("Класс:"), 1, 0);
    filterLayout->addWidget(classFilter, 1, 1);
    filterLayout->addWidget(new QLabel("Уровень:"), 1, 2);
    filterLayout->addWidget(levelFilter, 1, 3);
    filterLayout->addWidget(new QLabel("Школа:"), 1, 4);
    filterLayout->addWidget(schoolFilter, 1, 5);

    // Row 2: Subclass, Time, Concentration/Ritual
    subclassFilter = new QComboBox(this);
    subclassFilter->addItem("Все подклассы");
    connect(subclassFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpellBookWidget::filterSpells);

    castingTimeFilter = new QComboBox(this);
    castingTimeFilter->addItems({"Любое", "Действие", "Бонусное действие", "Реакция", "Минута+", "Час+"});
    connect(castingTimeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpellBookWidget::filterSpells);

    checkRitual = new QCheckBox("Ритуал", this);
    checkRitual->setToolTip("Показать только ритуалы");
    connect(checkRitual, &QCheckBox::toggled, this, &SpellBookWidget::filterSpells);

    checkConcentration = new QCheckBox("Концентрация", this);
    checkConcentration->setToolTip("Показать только заклинания с концентрацией");
    connect(checkConcentration, &QCheckBox::toggled, this, &SpellBookWidget::filterSpells);

    filterLayout->addWidget(new QLabel("Подкласс:"), 2, 0);
    filterLayout->addWidget(subclassFilter, 2, 1);
    filterLayout->addWidget(new QLabel("Время:"), 2, 2);
    filterLayout->addWidget(castingTimeFilter, 2, 3);
    filterLayout->addWidget(checkRitual, 2, 4);
    filterLayout->addWidget(checkConcentration, 2, 5);

    // Row 3: Components
    checkV = new QCheckBox("V", this);
    checkS = new QCheckBox("S", this);
    checkM = new QCheckBox("M", this);
    checkV->setToolTip("Включая Вербальный компонент");
    checkS->setToolTip("Включая Соматический компонент");
    checkM->setToolTip("Включая Материальный компонент");
    // By default all unchecked (ignore filter)
    connect(checkV, &QCheckBox::toggled, this, &SpellBookWidget::filterSpells);
    connect(checkS, &QCheckBox::toggled, this, &SpellBookWidget::filterSpells);
    connect(checkM, &QCheckBox::toggled, this, &SpellBookWidget::filterSpells);

    QHBoxLayout *compLayout = new QHBoxLayout();
    compLayout->addWidget(new QLabel("Компоненты:"));
    compLayout->addWidget(checkV);
    compLayout->addWidget(checkS);
    compLayout->addWidget(checkM);
    compLayout->addStretch();
    
    filterLayout->addLayout(compLayout, 3, 0, 1, 6);

    // Prevent filter group from expanding vertically
    filterGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    mainLayout->addWidget(filterGroup);

    // Content Area (Splitter)
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // Left: List of Spells
    spellList = new QListWidget(this);
    connect(spellList, &QListWidget::itemClicked, this, &SpellBookWidget::onSpellSelected);
    splitter->addWidget(spellList);

    // Right: Spell Details
    QWidget *detailsWidget = new QWidget(this);
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsWidget);
    
    nameLabel = new QLabel("Выберите заклинание", this);
    QFont titleFont = nameLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(14);
    nameLabel->setFont(titleFont);
    nameLabel->setWordWrap(true);
    
    infoLabel = new QLabel("", this);
    infoLabel->setStyleSheet("color: #888; font-style: italic;");
    
    descriptionText = new QTextEdit(this);
    descriptionText->setReadOnly(true);
    
    detailsLayout->addWidget(nameLabel);
    detailsLayout->addWidget(infoLabel);
    detailsLayout->addWidget(descriptionText);
    
    splitter->addWidget(detailsWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    // Give the splitter all remaining vertical space
    mainLayout->addWidget(splitter, 1);
}

void SpellBookWidget::loadAllSpells()
{
    m_allSpells = DatabaseManager::instance().getAllSpells();
    
    // Populate subclass dropdown
    QStringList subclasses;
    for (const Spell &spell : m_allSpells) {
        if (spell.subclasses.isEmpty()) continue;
        // Split by comma in case multiple subclasses listed
        QStringList parts = spell.subclasses.split(", ", Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            if (!subclasses.contains(part)) {
                subclasses.append(part);
            }
        }
    }
    subclasses.sort();
    
    // Block signals to prevent triggering filter 50 times
    subclassFilter->blockSignals(true);
    subclassFilter->clear();
    subclassFilter->addItem("Все подклассы");
    subclassFilter->addItems(subclasses);
    subclassFilter->blockSignals(false);
}

void SpellBookWidget::filterSpells()
{
    spellList->clear();
    
    QString searchName = searchBar->text().toLower();
    QString searchClass = classFilter->currentIndex() == 0 ? "" : classFilter->currentText().toLower();
    int searchLevel = levelFilter->currentIndex() - 1; // 0 index = All, 1 = 0(Cantrip), 2 = 1, etc.
    QString searchSchool = schoolFilter->currentIndex() == 0 ? "" : schoolFilter->currentText().toLower();
    QString searchSubclass = subclassFilter->currentIndex() == 0 ? "" : subclassFilter->currentText().toLower();
    
    // Casting Time Logic
    // 0=Any, 1=Action, 2=Bonus, 3=Reaction, 4=Minute+, 5=Hour+
    int timeIdx = castingTimeFilter->currentIndex();
    
    bool reqRitual = checkRitual->isChecked();
    bool reqConc = checkConcentration->isChecked();
    bool reqV = checkV->isChecked();
    bool reqS = checkS->isChecked();
    bool reqM = checkM->isChecked();

    // Сортировка по алфавиту перед фильтрацией
    std::sort(m_allSpells.begin(), m_allSpells.end(), [](const Spell &a, const Spell &b) {
        return a.name < b.name;
    });

    for (const Spell &spell : m_allSpells) {
        // Name Filter
        if (!searchName.isEmpty() && !spell.name.toLower().contains(searchName)) continue;
        
        // Class Filter
        if (!searchClass.isEmpty() && !spell.classes.toLower().contains(searchClass)) continue;
        
        // Level Filter
        if (searchLevel >= 0 && spell.level != searchLevel) continue;
        
        // School Filter
        if (!searchSchool.isEmpty() && !spell.school.toLower().contains(searchSchool)) continue;

        // Subclass Filter
        if (!searchSubclass.isEmpty() && !spell.subclasses.toLower().contains(searchSubclass)) continue;

        // Casting Time Filter
        // Simple string matching based on Russian keywords
        QString cTime = spell.castingTime.toLower();
        if (timeIdx == 1 && !cTime.contains("действие")) continue; // Action, but check for "бонусное действие" conflict?
        if (timeIdx == 1 && cTime.contains("бонусное")) continue; // "Бонусное действие" contains "действие", so exclude it for pure Action
        
        if (timeIdx == 2 && !cTime.contains("бонусное")) continue;
        if (timeIdx == 3 && !cTime.contains("реакция")) continue;
        if (timeIdx == 4 && !(cTime.contains("минут") || cTime.contains("час"))) continue; // Loose matching
        if (timeIdx == 5 && !cTime.contains("час")) continue;

        // Ritual / Conc
        if (reqRitual && !spell.ritual) continue;
        if (reqConc && !spell.duration.toLower().contains("концентрация")) continue;

        // Components
        // Checkboxes logic: The user wants spells that HAVE these components.
        // Or "Only these"? Let's assume "Must have V if V checked".
        // Components string example: "В, С, М (кусок...)".
        QString comps = spell.components.toLower();
        bool hasV = comps.contains("в"); // Russian B
        bool hasS = comps.contains("с"); // Russian C
        bool hasM = comps.contains("м"); // Russian M
        
        if (reqV && !hasV) continue;
        if (reqS && !hasS) continue;
        if (reqM && !hasM) continue;

        QListWidgetItem *item = new QListWidgetItem(spell.name);
        // Store data to retrieve later
        item->setData(Qt::UserRole, spell.name); 
        item->setData(Qt::UserRole + 1, spell.level);
        item->setData(Qt::UserRole + 2, spell.school);
        item->setData(Qt::UserRole + 3, spell.description);
        item->setData(Qt::UserRole + 4, spell.ritual);
        item->setData(Qt::UserRole + 5, spell.castingTime);
        item->setData(Qt::UserRole + 6, spell.range);
        item->setData(Qt::UserRole + 7, spell.components);
        item->setData(Qt::UserRole + 8, spell.duration);
        item->setData(Qt::UserRole + 9, spell.classes);
        item->setData(Qt::UserRole + 10, spell.upper);
        item->setData(Qt::UserRole + 11, spell.subclasses);
        item->setData(Qt::UserRole + 12, spell.source);
        spellList->addItem(item);
    }
}

void SpellBookWidget::onSpellSelected(QListWidgetItem *item)
{
    if (!item) return;
    
    QString name = item->data(Qt::UserRole).toString();
    int level = item->data(Qt::UserRole + 1).toInt();
    QString school = item->data(Qt::UserRole + 2).toString();
    QString desc = item->data(Qt::UserRole + 3).toString();
    bool ritual = item->data(Qt::UserRole + 4).toBool();
    QString castingTime = item->data(Qt::UserRole + 5).toString();
    QString range = item->data(Qt::UserRole + 6).toString();
    QString components = item->data(Qt::UserRole + 7).toString();
    QString duration = item->data(Qt::UserRole + 8).toString();
    QString classes = item->data(Qt::UserRole + 9).toString();
    QString upper = item->data(Qt::UserRole + 10).toString();
    QString subclasses = item->data(Qt::UserRole + 11).toString();
    QString source = item->data(Qt::UserRole + 12).toString();
    
    nameLabel->setText(name);
    
    QString levelText = (level == 0) ? "Заговор" : QString("%1 уровень").arg(level);
    QString ritualText = ritual ? " (Ритуал)" : "";
    infoLabel->setText(QString("%1, %2%3").arg(levelText, school, ritualText));
    
    QString html = QString(
        "<p><b>Время накладывания:</b> %1</p>"
        "<p><b>Дистанция:</b> %2</p>"
        "<p><b>Компоненты:</b> %3</p>"
        "<p><b>Длительность:</b> %4</p>"
        "<p><b>Классы:</b> %5</p>"
    ).arg(castingTime, range, components, duration, classes);

    if (!subclasses.isEmpty()) {
        html += QString("<p><b>Подклассы:</b> %1</p>").arg(subclasses);
    }
    
    if (!source.isEmpty()) {
        html += QString("<p><b>Источник:</b> %1</p>").arg(source);
    }

    html += QString("<hr>%1").arg(desc);

    if (!upper.isEmpty()) {
        html += QString("<hr><p><b>На более высоких уровнях:</b> %1</p>").arg(upper);
    }

    descriptionText->setHtml(html);
}
