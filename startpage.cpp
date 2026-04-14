#include "startpage.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QScrollArea>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

StartPage::StartPage(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *titleLabel = new QLabel("Welcome to DnD Helper", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont font = titleLabel->font();
    font.setPointSize(16);
    font.setBold(true);
    titleLabel->setFont(font);

    createButton = new QPushButton("Create New Campaign", this);
    createButton->setMinimumHeight(50); // Make it prominent
    connect(createButton, &QPushButton::clicked, this, &StartPage::onCreateCampaignClicked);

    QLabel *listLabel = new QLabel("Your Campaigns:", this);
    QFont listFont = listLabel->font();
    listFont.setPointSize(12);
    listLabel->setFont(listFont);

    // Scroll Area for campaigns
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    campaignsContainer = new QWidget();
    campaignsLayout = new QVBoxLayout(campaignsContainer);
    campaignsLayout->setAlignment(Qt::AlignTop);
    campaignsLayout->setSpacing(10);
    
    scrollArea->setWidget(campaignsContainer);

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(createButton);
    mainLayout->addSpacing(20);
    mainLayout->addWidget(listLabel);
    mainLayout->addWidget(scrollArea);

    loadCampaigns();
}

void StartPage::loadCampaigns()
{
    clearLayout(campaignsLayout);

    QDir dir("campaigns");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QStringList campaigns = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &campaignName : campaigns) {
        QWidget *rowWidget = new QWidget(this);
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        QPushButton *btn = new QPushButton(campaignName, this);
        btn->setMinimumHeight(40);
        // Connect using a lambda to capture the campaign name
        connect(btn, &QPushButton::clicked, [this, campaignName]() {
            onCampaignButtonClicked(campaignName);
        });

        QPushButton *deleteBtn = new QPushButton("X", this);
        deleteBtn->setFixedSize(40, 40);
        deleteBtn->setStyleSheet("background-color: #ff9999; font-weight: bold;");
        deleteBtn->setToolTip("Delete Campaign");
        connect(deleteBtn, &QPushButton::clicked, [this, campaignName]() {
            onDeleteCampaignClicked(campaignName);
        });

        rowLayout->addWidget(btn);
        rowLayout->addWidget(deleteBtn);

        campaignsLayout->addWidget(rowWidget);
    }
}

void StartPage::clearLayout(QLayout *layout)
{
    QLayoutItem *item;
    while ((item = layout->takeAt(0))) {
        if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }
}

void StartPage::onCreateCampaignClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "New Campaign", "Campaign Name:", QLineEdit::Normal, "", &ok);
    if (ok && !name.isEmpty()) {
        // Ask for role
        QStringList roles;
        roles << "Player" << "Game Master";
        QString role = QInputDialog::getItem(this, "Select Role", "Role:", roles, 0, false, &ok);
        
        if (ok) {
            QDir dir;
            QString campaignPath = "campaigns/" + name;
            if (dir.mkpath(campaignPath)) {
                // Save role to campaign.json
                QJsonObject campaignData;
                campaignData["role"] = role;
                
                QFile file(campaignPath + "/campaign.json");
                if (file.open(QIODevice::WriteOnly)) {
                    QJsonDocument doc(campaignData);
                    file.write(doc.toJson());
                    file.close();
                }

                loadCampaigns(); // Refresh list
                bool isMaster = (role == "Game Master");
                emit campaignSelected(name, isMaster);
            } else {
                QMessageBox::warning(this, "Error", "Could not create campaign directory.");
            }
        }
    }
}

void StartPage::onCampaignButtonClicked(const QString &campaignName)
{
    QString campaignPath = "campaigns/" + campaignName + "/campaign.json";
    QFile file(campaignPath);
    
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        
        QString role = obj["role"].toString();
        bool isMaster = (role == "Game Master");
        emit campaignSelected(campaignName, isMaster);
        return;
    }

    // Fallback for old campaigns
    QStringList roles;
    roles << "Player" << "Game Master";
    bool ok;
    QString role = QInputDialog::getItem(this, "Select Role", "Role:", roles, 0, false, &ok);
    
    if (ok) {
        bool isMaster = (role == "Game Master");
        emit campaignSelected(campaignName, isMaster);
    }
}

void StartPage::onDeleteCampaignClicked(const QString &campaignName)
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Delete Campaign", 
                                  "Are you sure you want to delete campaign '" + campaignName + "'?\nAll data will be lost permanently.",
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        QDir dir("campaigns/" + campaignName);
        if (dir.removeRecursively()) {
            loadCampaigns();
        } else {
            QMessageBox::warning(this, "Error", "Could not delete campaign directory.");
        }
    }
}
