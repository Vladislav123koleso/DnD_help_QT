#include "masterpage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QMenu>
#include <QListWidget>
#include <QStackedWidget>
#include "spellbookwidget.h"
#include "itembookwidget.h"
#include "bestiarywidget.h"

MasterPage::MasterPage(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void MasterPage::setCampaign(const QString &campaignName)
{
    currentCampaign = campaignName;
}

void MasterPage::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Left Side Container
    QWidget *leftPanel = new QWidget(this);
    leftPanel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // Toggle Sidebar Button
    QPushButton *toggleBtn = new QPushButton("◀", this);
    toggleBtn->setFixedSize(40, 40);
    toggleBtn->setStyleSheet(
        "QPushButton { font-size: 18px; font-weight: bold; border: none; background-color: #333; color: white; }"
        "QPushButton:hover { background-color: #444; }"
        "QToolTip { font-size: 12px; font-weight: normal; }"
    );
    toggleBtn->setToolTip("Свернуть/Развернуть меню");
    
    // Add button aligned to left so it doesn't move when panel width changes
    leftLayout->addWidget(toggleBtn, 0, Qt::AlignLeft);

    // Navigation List
    navBar = new QListWidget(this);
    navBar->setFixedWidth(180);
    navBar->setStyleSheet(
        "QListWidget { border: none; background-color: #2b2b2b; color: #e0e0e0; outline: none; }"
        "QListWidget::item { padding: 12px; border-bottom: 1px solid #3d3d3d; }"
        "QListWidget::item:selected { background-color: #404040; color: #ffffff; border-left: 3px solid #3498db; }"
        "QListWidget::item:hover { background-color: #353535; }"
    );
    leftLayout->addWidget(navBar, 1);
    leftLayout->addStretch(0); // Push content up when navbar is hidden

    connect(toggleBtn, &QPushButton::clicked, [this, toggleBtn]() {
        bool isVisible = navBar->isVisible();
        navBar->setVisible(!isVisible);
        toggleBtn->setText(isVisible ? "▶" : "◀");
    });

    mainLayout->addWidget(leftPanel);

    // Right Side Container
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // Top Bar for Menu Button
    QHBoxLayout *topBar = new QHBoxLayout();
    topBar->addStretch(); // Push to right
    
    // Menu Button (Hamburger)
    QToolButton *menuBtn = new QToolButton(this);
    menuBtn->setText("☰");
    menuBtn->setAutoRaise(true);
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    menuBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    menuBtn->setFixedSize(40, 40);
    QFont btnFont = menuBtn->font();
    btnFont.setPointSize(14);
    menuBtn->setFont(btnFont);

    QMenu *menu = new QMenu(this);
    menu->addAction("Option 1");
    menu->addAction("Option 2");
    menu->addSeparator();
    QAction *mainMenuAction = menu->addAction("Главное меню");

    menuBtn->setMenu(menu);
    connect(mainMenuAction, &QAction::triggered, this, &MasterPage::mainMenuRequested);
    
    topBar->addWidget(menuBtn);
    topBar->setContentsMargins(0, 0, 10, 0);

    rightLayout->addLayout(topBar);

    // Content Stack
    contentStack = new QStackedWidget(this);

    // Create Tabs/Pages
    QStringList tabNames = {
        "Заметки",
        "Создание NPC",
        "Список Созданных НПС",
        "Список Заклинаний",
        "Список Предметов",
        "Список Оружия и доспехов",
        "Бестиарий",
        "Генератор Названий\\Имен",
        "Генератор Артефактов"
    };

    for (const QString &name : tabNames) {
        // Add item to list
        navBar->addItem(name);

        // Add page to stack
        QWidget *page = new QWidget();
        QVBoxLayout *pageLayout = new QVBoxLayout(page);
        
        if (name == "Список Заклинаний") {
            pageLayout->addWidget(new SpellBookWidget(this));
        } else if (name == "Список Предметов") {
            // General Items only
            pageLayout->addWidget(new ItemBookWidget(ItemBookWidget::GeneralItems, this));
        } else if (name == "Список Оружия и доспехов") {
            // Weapons and Armor only
            pageLayout->addWidget(new ItemBookWidget(ItemBookWidget::WeaponsAndArmor, this));
        } else if (name == "Бестиарий") {
            pageLayout->addWidget(new BestiaryWidget(this));
        } else {
            QLabel *label = new QLabel(name + " Placeholder");
            label->setAlignment(Qt::AlignCenter);
            pageLayout->addWidget(label);
        }
        
        contentStack->addWidget(page);
    }
    
    rightLayout->addWidget(contentStack);

    // Connect selection
    connect(navBar, &QListWidget::currentRowChanged, contentStack, &QStackedWidget::setCurrentIndex);

    // Select first item by default
    if (navBar->count() > 0) {
        navBar->setCurrentRow(0);
    }

    mainLayout->addWidget(rightPanel);
}
