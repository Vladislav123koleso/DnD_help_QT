#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    startPage = new StartPage(this);
    playerPage = new PlayerPage(this);
    masterPage = new MasterPage(this);

    stackedWidget->addWidget(startPage);
    stackedWidget->addWidget(playerPage);
    stackedWidget->addWidget(masterPage);

    stackedWidget->setCurrentWidget(startPage);

    connect(startPage, &StartPage::campaignSelected, this, &MainWindow::onCampaignSelected);
    connect(playerPage, &PlayerPage::mainMenuRequested, this, &MainWindow::onMainMenuRequested);
    connect(masterPage, &MasterPage::mainMenuRequested, this, &MainWindow::onMainMenuRequested);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onCampaignSelected(const QString &campaignName, bool isMaster)
{
    if (isMaster) {
        masterPage->setCampaign(campaignName);
        stackedWidget->setCurrentWidget(masterPage);
    } else {
        playerPage->setCampaign(campaignName);
        stackedWidget->setCurrentWidget(playerPage);
    }
}

void MainWindow::onMainMenuRequested()
{
    stackedWidget->setCurrentWidget(startPage);
}
