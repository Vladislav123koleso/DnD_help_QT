#include "playerpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QRandomGenerator>
#include "character.h"
#include "spellbookwidget.h"
#include "itembookwidget.h"

PlayerPage::PlayerPage(QWidget *parent)
    : QWidget(parent),
      currentCharacter(nullptr),
      targetCharacterLevel(1),
      allocatedClassLevels(0)
{
    setupUi();
}

void PlayerPage::setCampaign(const QString &campaignName)
{
    currentCampaign = campaignName;
    // Load character data for this campaign if exists
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
    menu->addAction("Option 1"); // Placeholder
    menu->addAction("Option 2"); // Placeholder
    menu->addSeparator();
    QAction *mainMenuAction = menu->addAction("Main Menu");

    menuBtn->setMenu(menu);
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
    connect(createCharBtn, &QPushButton::clicked, this, &PlayerPage::startCharacterCreation);
    
    charControlsLayout->addWidget(new QLabel("Информация о персонаже"));
    charControlsLayout->addStretch();
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
    connect(backBtn, &QPushButton::clicked, this, &PlayerPage::showCharacterInfo);
    
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
    connect(classBackBtn, &QPushButton::clicked, this, [this](){ charStack->setCurrentIndex(1); });
    
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

    targetCharacterLevel = chosenLevel;

    // Create a new empty character template
    if (currentCharacter) {
        // Option confirm
    } else {
        currentCharacter = new Character(this);
    }

    resetCreationProgress();
    currentCharacter->level = targetCharacterLevel;
    
    // Assign to sheet to see live updates (even if empty)
    characterSheet->setCharacter(currentCharacter);
    
    // Switch to race selection
    charStack->setCurrentIndex(1);
    
    // Reset race page to list view
    racePage->showList();
}


void PlayerPage::showCharacterInfo()
{
    charStack->setCurrentIndex(0);
    characterSheet->setCharacter(currentCharacter);
}

void PlayerPage::resetCreationProgress()
{
    allocatedClassLevels = 0;
    selectedClassLevels.clear();
    if (currentCharacter) {
        currentCharacter->setCharacterClass("");
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
    QMap<QString, int>::const_iterator it = selectedClassLevels.constBegin();
    for (; it != selectedClassLevels.constEnd(); ++it) {
        parts << QString("%1 %2").arg(it.key()).arg(it.value());
    }

    currentCharacter->setCharacterClass(parts.join(" / "));
}

bool PlayerPage::chooseAndApplyBaseAbilityScores()
{
    if (!currentCharacter) {
        return false;
    }

    QMessageBox modeBox(this);
    modeBox.setIcon(QMessageBox::Question);
    modeBox.setWindowTitle("Распределение характеристик");
    modeBox.setText("Выберите способ определения стартовых характеристик:");

    QPushButton *presetBtn = modeBox.addButton("Распределить набор 8, 10, 12, 14, 15, 16", QMessageBox::AcceptRole);
    QPushButton *rollBtn = modeBox.addButton("Сгенерировать броском d20", QMessageBox::AcceptRole);
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
        QList<int> pool = {8, 10, 12, 14, 15, 16};

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
                return false;
            }

            int score = selected.toInt();
            chosenScores[statName] = score;
            pool.removeOne(score);
        }
    } else if (modeBox.clickedButton() == rollBtn) {
        QStringList rollLines;
        for (const QString &statName : statNames) {
            int score = QRandomGenerator::global()->bounded(1, 21);
            chosenScores[statName] = score;
            rollLines << QString("%1: %2").arg(statName).arg(score);
        }

        QMessageBox::information(
            this,
            "Результаты бросков d20",
            QString("Сгенерированные характеристики:\n%1").arg(rollLines.join("\n")));
    } else {
        return false;
    }

    currentCharacter->strength = chosenScores["Сила"];
    currentCharacter->dexterity = chosenScores["Ловкость"];
    currentCharacter->constitution = chosenScores["Телосложение"];
    currentCharacter->intelligence = chosenScores["Интеллект"];
    currentCharacter->wisdom = chosenScores["Мудрость"];
    currentCharacter->charisma = chosenScores["Харизма"];

    return true;
}

void PlayerPage::applyRaceAbilityBonuses(const Race &race)
{
    if (!currentCharacter) {
        return;
    }

    for (auto it = race.abilityScoreIncrease.begin(); it != race.abilityScoreIncrease.end(); ++it) {
        if (it.key() == "Strength") {
            currentCharacter->strength = Character::clampAbilityScore(currentCharacter->strength + it.value());
        } else if (it.key() == "Dexterity") {
            currentCharacter->dexterity = Character::clampAbilityScore(currentCharacter->dexterity + it.value());
        } else if (it.key() == "Constitution") {
            currentCharacter->constitution = Character::clampAbilityScore(currentCharacter->constitution + it.value());
        } else if (it.key() == "Intelligence") {
            currentCharacter->intelligence = Character::clampAbilityScore(currentCharacter->intelligence + it.value());
        } else if (it.key() == "Wisdom") {
            currentCharacter->wisdom = Character::clampAbilityScore(currentCharacter->wisdom + it.value());
        } else if (it.key() == "Charisma") {
            currentCharacter->charisma = Character::clampAbilityScore(currentCharacter->charisma + it.value());
        }
    }
}

void PlayerPage::onRaceChosen(const Race &race)
{
    if (!currentCharacter) return;

    if (!chooseAndApplyBaseAbilityScores()) {
        return;
    }
    
    // Apply Race Data to Character
    currentCharacter->setRace(race.name);
    currentCharacter->size = race.size;
    currentCharacter->speed = race.speed;
    currentCharacter->flyingSpeed = race.flyingSpeed;
    currentCharacter->languages = race.languages;
    
    // Apply racial bonuses with cap 20
    applyRaceAbilityBonuses(race);
    
    // Add Traits
    currentCharacter->traits = race.traits;
    
    // Log for debug
    qDebug() << "Character Race Selected:" << race.name;
    qDebug() << "New Stats: Str" << currentCharacter->strength << "Dex" << currentCharacter->dexterity 
             << "Con" << currentCharacter->constitution << "Int" << currentCharacter->intelligence
             << "Wis" << currentCharacter->wisdom << "Cha" << currentCharacter->charisma;

    // Move to next step (Class Selection - Index 2)
    charStack->setCurrentIndex(2);
    classPage->showList();
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

    allocatedClassLevels += classLevel;
    selectedClassLevels[cls.name] += classLevel;
    updateCharacterClassSummary();

    if (selectedClassLevels.size() == 1) {
        currentCharacter->hitDie = cls.hitDie;
    }

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
            return;
        }

        if (choiceBox.clickedButton() == fillCurrentBtn) {
            selectedClassLevels[cls.name] += remaining;
            allocatedClassLevels += remaining;
            updateCharacterClassSummary();
        }
    }

    showCharacterInfo();
}



