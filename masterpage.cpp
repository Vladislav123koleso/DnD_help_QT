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
#include "noteswidget.h"
#include "namegeneratorwidget.h"
#include "artifactgeneratorwidget.h"

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
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QWidget *leftPanel = new QWidget(this);
    leftPanel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    QPushButton *toggleBtn = new QPushButton(QStringLiteral("◀"), this);
    toggleBtn->setFixedSize(40, 40);
    toggleBtn->setStyleSheet(
        "QPushButton { font-size: 18px; font-weight: bold; border: none; background-color: #333; color: white; }"
        "QPushButton:hover { background-color: #444; }"
        "QToolTip { font-size: 12px; font-weight: normal; }"
    );
    toggleBtn->setToolTip(QStringLiteral("Свернуть/Развернуть меню"));

    leftLayout->addWidget(toggleBtn, 0, Qt::AlignLeft);

    navBar = new QListWidget(this);
    navBar->setFixedWidth(180);
    navBar->setStyleSheet(
        "QListWidget { border: none; background-color: #2b2b2b; color: #e0e0e0; outline: none; }"
        "QListWidget::item { padding: 12px; border-bottom: 1px solid #3d3d3d; }"
        "QListWidget::item:selected { background-color: #404040; color: #ffffff; border-left: 3px solid #3498db; }"
        "QListWidget::item:hover { background-color: #353535; }"
    );
    leftLayout->addWidget(navBar, 1);
    leftLayout->addStretch(0);

    connect(toggleBtn, &QPushButton::clicked, [this, toggleBtn]() {
        const bool isVisible = navBar->isVisible();
        navBar->setVisible(!isVisible);
        toggleBtn->setText(isVisible ? QStringLiteral("▶") : QStringLiteral("◀"));
    });

    mainLayout->addWidget(leftPanel);

    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    QHBoxLayout *topBar = new QHBoxLayout();
    topBar->addStretch();

    QToolButton *menuBtn = new QToolButton(this);
    menuBtn->setText(QStringLiteral("☰"));
    menuBtn->setAutoRaise(true);
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    menuBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    menuBtn->setFixedSize(40, 40);
    QFont btnFont = menuBtn->font();
    btnFont.setPointSize(14);
    menuBtn->setFont(btnFont);

    QMenu *menu = new QMenu(this);
    menu->addAction(QStringLiteral("Option 1"));
    menu->addAction(QStringLiteral("Option 2"));
    menu->addSeparator();
    QAction *mainMenuAction = menu->addAction(QStringLiteral("Главное меню"));

    menuBtn->setMenu(menu);
    connect(mainMenuAction, &QAction::triggered, this, &MasterPage::mainMenuRequested);

    topBar->addWidget(menuBtn);
    topBar->setContentsMargins(0, 0, 10, 0);
    rightLayout->addLayout(topBar);

    contentStack = new QStackedWidget(this);

    const QStringList tabNames = {
        QStringLiteral("Заметки"),
        QStringLiteral("Создание NPC"),
        QStringLiteral("Список Созданных НПС"),
        QStringLiteral("Список Заклинаний"),
        QStringLiteral("Список Предметов"),
        QStringLiteral("Список Оружия и доспехов"),
        QStringLiteral("Бестиарий"),
        QStringLiteral("Генератор Названий\\Имен"),
        QStringLiteral("Генератор Артефактов")
    };

    for (int tabIndex = 0; tabIndex < tabNames.size(); ++tabIndex) {
        const QString &name = tabNames.at(tabIndex);
        navBar->addItem(name);

        QWidget *page = new QWidget();
        QVBoxLayout *pageLayout = new QVBoxLayout(page);

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
            QLabel *label = new QLabel(name + QStringLiteral(" Placeholder"));
            label->setAlignment(Qt::AlignCenter);
            pageLayout->addWidget(label);
        }

        contentStack->addWidget(page);
    }

    rightLayout->addWidget(contentStack);

    connect(navBar, &QListWidget::currentRowChanged, contentStack, &QStackedWidget::setCurrentIndex);

    if (navBar->count() > 0) {
        navBar->setCurrentRow(0);
    }

    mainLayout->addWidget(rightPanel);
}
