#include "masterpage.h"

#include "artifactgeneratorwidget.h"
#include "bestiarywidget.h"
#include "itembookwidget.h"
#include "namegeneratorwidget.h"
#include "spellbookwidget.h"

#include <QAction>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

MasterPage::MasterPage(QWidget *parent) : QWidget(parent)
{
    setupUi();
}

void MasterPage::setCampaign(const QString &campaignName)
{
    currentCampaign = campaignName.trimmed();
    const QString scopeSuffix = currentCampaign.isEmpty() ? QStringLiteral("default") : currentCampaign;
    if (notesWidget) {
        notesWidget->setStorageScope(QStringLiteral("master_%1").arg(scopeSuffix));
    }
    if (npcCreatorWidget) {
        npcCreatorWidget->setStorageScope(QStringLiteral("master_%1").arg(scopeSuffix));
    }
    if (npcListWidget) {
        npcListWidget->setStorageScope(QStringLiteral("master_%1").arg(scopeSuffix));
    }
}

void MasterPage::setupUi()
{
    setObjectName(QStringLiteral("MasterPage"));

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    auto *shellSplitter = new QSplitter(Qt::Horizontal, this);
    shellSplitter->setChildrenCollapsible(false);

    auto *leftPanel = new QWidget(shellSplitter);
    leftPanel->setObjectName(QStringLiteral("SidebarPanel"));
    leftPanel->setProperty("sidebarCollapsed", false);
    leftPanel->setMinimumWidth(210);
    leftPanel->setMaximumWidth(340);
    leftPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(8);

    auto *toggleBtn = new QPushButton(QStringLiteral("\u25C0"), leftPanel);
    toggleBtn->setObjectName(QStringLiteral("SidebarToggleBtn"));
    toggleBtn->setToolTip(QStringLiteral("Свернуть/развернуть меню"));
    toggleBtn->setFixedSize(34, 34);
    leftLayout->addWidget(toggleBtn, 0, Qt::AlignLeft);

    navBar = new QListWidget(leftPanel);
    navBar->setObjectName(QStringLiteral("SidebarNav"));
    navBar->setMinimumWidth(194);
    navBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    leftLayout->addWidget(navBar, 1);
    leftLayout->addStretch(1);

    connect(toggleBtn, &QPushButton::clicked, [this, toggleBtn, leftPanel]() {
        const bool isVisible = navBar->isVisible();
        navBar->setVisible(!isVisible);
        leftPanel->setProperty("sidebarCollapsed", isVisible);
        if (isVisible) {
            leftPanel->setMinimumWidth(54);
            leftPanel->setMaximumWidth(64);
        } else {
            const bool tight = window() && window()->property("uxTight").toBool();
            leftPanel->setMinimumWidth(tight ? 170 : 210);
            leftPanel->setMaximumWidth(tight ? 260 : 340);
        }
        toggleBtn->setText(isVisible ? QStringLiteral("\u25B6") : QStringLiteral("\u25C0"));
    });

    auto *rightPanel = new QWidget(shellSplitter);
    rightPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(8);

    auto *topBar = new QHBoxLayout();
    topBar->addStretch(1);

    auto *menuBtn = new QToolButton(rightPanel);
    menuBtn->setObjectName(QStringLiteral("SidebarMenuBtn"));
    menuBtn->setText(QStringLiteral("\u2630"));
    menuBtn->setAutoRaise(true);
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    menuBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    menuBtn->setFixedSize(34, 34);
    QFont btnFont = menuBtn->font();
    btnFont.setPointSize(13);
    btnFont.setBold(true);
    menuBtn->setFont(btnFont);

    auto *menu = new QMenu(menuBtn);
    QAction *mainMenuAction = menu->addAction(QStringLiteral("Главное меню"));
    menuBtn->setMenu(menu);
    connect(mainMenuAction, &QAction::triggered, this, &MasterPage::mainMenuRequested);

    topBar->addWidget(menuBtn);
    rightLayout->addLayout(topBar);

    contentStack = new QStackedWidget(rightPanel);

    const QStringList tabNames = {
        QStringLiteral("Заметки"),
        QStringLiteral("Создание NPC"),
        QStringLiteral("Список созданных НПС"),
        QStringLiteral("Список заклинаний"),
        QStringLiteral("Список предметов"),
        QStringLiteral("Список оружия и доспехов"),
        QStringLiteral("Бестиарий"),
        QStringLiteral("Генератор названий/имен"),
        QStringLiteral("Генератор артефактов")
    };

    for (int tabIndex = 0; tabIndex < tabNames.size(); ++tabIndex) {
        navBar->addItem(tabNames.at(tabIndex));

        auto *page = new QWidget();
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->setSpacing(0);

        if (tabIndex == 0) {
            notesWidget = new NotesWidget(this);
            pageLayout->addWidget(notesWidget);
        } else if (tabIndex == 1) {
            npcCreatorWidget = new NpcCreatorWidget(this);
            pageLayout->addWidget(npcCreatorWidget);
        } else if (tabIndex == 2) {
            npcListWidget = new NpcListWidget(this);
            pageLayout->addWidget(npcListWidget);
        } else if (tabIndex == 3) {
            pageLayout->addWidget(new SpellBookWidget(this));
        } else if (tabIndex == 4) {
            pageLayout->addWidget(new ItemBookWidget(ItemBookWidget::GeneralItems, this));
        } else if (tabIndex == 5) {
            pageLayout->addWidget(new ItemBookWidget(ItemBookWidget::WeaponsAndArmor, this));
        } else if (tabIndex == 6) {
            pageLayout->addWidget(new BestiaryWidget(this));
        } else if (tabIndex == 7) {
            nameGeneratorWidget = new NameGeneratorWidget(this);
            pageLayout->addWidget(nameGeneratorWidget);
        } else if (tabIndex == 8) {
            artifactGeneratorWidget = new ArtifactGeneratorWidget(this);
            pageLayout->addWidget(artifactGeneratorWidget);
        } else {
            auto *label = new QLabel(tabNames.at(tabIndex), page);
            label->setAlignment(Qt::AlignCenter);
            pageLayout->addWidget(label);
        }

        contentStack->addWidget(page);
    }

    rightLayout->addWidget(contentStack, 1);

    connect(navBar, &QListWidget::currentRowChanged, contentStack, &QStackedWidget::setCurrentIndex);

    if (navBar->count() > 0) {
        navBar->setCurrentRow(0);
    }

    shellSplitter->addWidget(leftPanel);
    shellSplitter->addWidget(rightPanel);
    shellSplitter->setStretchFactor(0, 0);
    shellSplitter->setStretchFactor(1, 1);
    shellSplitter->setSizes({240, 1080});

    mainLayout->addWidget(shellSplitter, 1);
}
