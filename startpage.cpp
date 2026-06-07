#include "startpage.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QScrollArea>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFrame>
#include <QSizePolicy>

StartPage::StartPage(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("StartPage"));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(22, 18, 22, 18);
    mainLayout->setSpacing(14);

    QFrame *heroCard = new QFrame(this);
    heroCard->setObjectName(QStringLiteral("Card"));
    QVBoxLayout *heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(18, 16, 18, 16);
    heroLayout->setSpacing(10);

    QLabel *titleLabel = new QLabel(QStringLiteral("DnD Helper"), heroCard);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont font = titleLabel->font();
    font.setPointSize(22);
    font.setBold(true);
    titleLabel->setFont(font);

    QLabel *subtitleLabel = new QLabel(
        QStringLiteral("Управляйте кампаниями, персонажами и инструментами мастера в одном месте."),
        heroCard);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setProperty("role", QStringLiteral("muted"));

    createButton = new QPushButton(QStringLiteral("Создать новую кампанию"), heroCard);
    createButton->setProperty("variant", QStringLiteral("accent"));
    createButton->setMinimumHeight(44);
    createButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(createButton, &QPushButton::clicked, this, &StartPage::onCreateCampaignClicked);

    heroLayout->addWidget(titleLabel);
    heroLayout->addWidget(subtitleLabel);
    heroLayout->addWidget(createButton);

    QLabel *listLabel = new QLabel(QStringLiteral("Ваши кампании"), this);
    QFont listFont = listLabel->font();
    listFont.setPointSize(13);
    listFont.setBold(true);
    listLabel->setFont(listFont);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    campaignsContainer = new QWidget();
    campaignsLayout = new QVBoxLayout(campaignsContainer);
    campaignsLayout->setAlignment(Qt::AlignTop);
    campaignsLayout->setSpacing(10);
    
    scrollArea->setWidget(campaignsContainer);

    mainLayout->addWidget(heroCard);
    mainLayout->addWidget(listLabel);
    mainLayout->addWidget(scrollArea, 1);

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
        rowLayout->setSpacing(8);

        QPushButton *btn = new QPushButton(campaignName, this);
        btn->setMinimumHeight(38);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(btn, &QPushButton::clicked, [this, campaignName]() {
            onCampaignButtonClicked(campaignName);
        });

        QPushButton *deleteBtn = new QPushButton(QStringLiteral("Удалить"), this);
        deleteBtn->setProperty("role", QStringLiteral("danger"));
        deleteBtn->setMinimumHeight(38);
        deleteBtn->setMinimumWidth(96);
        deleteBtn->setToolTip(QStringLiteral("Удалить кампанию"));
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
    QString name = QInputDialog::getText(
        this,
        QStringLiteral("Новая кампания"),
        QStringLiteral("Название кампании:"),
        QLineEdit::Normal,
        QString(),
        &ok);
    if (ok && !name.isEmpty()) {
        QStringList roles;
        roles << QStringLiteral("Игрок") << QStringLiteral("Мастер");
        QString role = QInputDialog::getItem(
            this,
            QStringLiteral("Выбор роли"),
            QStringLiteral("Роль:"),
            roles,
            0,
            false,
            &ok);
        
        if (ok) {
            QDir dir;
            QString campaignPath = "campaigns/" + name;
            if (dir.mkpath(campaignPath)) {
                QJsonObject campaignData;
                campaignData["role"] = role;
                
                QFile file(campaignPath + "/campaign.json");
                if (file.open(QIODevice::WriteOnly)) {
                    QJsonDocument doc(campaignData);
                    file.write(doc.toJson());
                    file.close();
                }

                loadCampaigns(); // Refresh list
                const bool isMaster = (role == QStringLiteral("Мастер") || role == QStringLiteral("Game Master"));
                emit campaignSelected(name, isMaster);
            } else {
                QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Не удалось создать директорию кампании."));
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
        const bool isMaster = (role == QStringLiteral("Мастер") || role == QStringLiteral("Game Master"));
        emit campaignSelected(campaignName, isMaster);
        return;
    }

    QStringList roles;
    roles << QStringLiteral("Игрок") << QStringLiteral("Мастер");
    bool ok;
    QString role = QInputDialog::getItem(
        this,
        QStringLiteral("Выбор роли"),
        QStringLiteral("Роль:"),
        roles,
        0,
        false,
        &ok);
    
    if (ok) {
        const bool isMaster = (role == QStringLiteral("Мастер") || role == QStringLiteral("Game Master"));
        emit campaignSelected(campaignName, isMaster);
    }
}

void StartPage::onDeleteCampaignClicked(const QString &campaignName)
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        this,
        QStringLiteral("Удаление кампании"),
        QStringLiteral("Удалить кампанию \"%1\"? Это действие нельзя отменить.").arg(campaignName),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        QDir dir("campaigns/" + campaignName);
        if (dir.removeRecursively()) {
            loadCampaigns();
        } else {
            QMessageBox::warning(this, QStringLiteral("Ошибка"), QStringLiteral("Не удалось удалить кампанию."));
        }
    }
}
