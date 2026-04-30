#include "classdetailspage.h"
#include <QFont>
#include <QDebug>
#include <QGroupBox>
#include <QPixmap>

namespace {

QString signedNumber(int value)
{
    return QString("%1%2").arg(value >= 0 ? "+" : "").arg(value);
}

}

ClassDetailsPage::ClassDetailsPage(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Header (Title)
    titleLabel = new QLabel(this);
    QFont titleFont("Arial", 24, QFont::Bold);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // Image
    imageLabel = new QLabel(this);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setFixedHeight(200); 
    mainLayout->addWidget(imageLabel);
    
    // Scroll Area for details
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget *contentWidget = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    
    // Description
    descriptionLabel = new QLabel(this);
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setStyleSheet("font-style: italic; margin-bottom: 20px;");
    contentLayout->addWidget(descriptionLabel);
    
    // Features Group
    QGroupBox *featuresGroup = new QGroupBox("Характеристики Класса", this);
    QVBoxLayout *featuresLayout = new QVBoxLayout(featuresGroup);
    
    hitDieLabel = new QLabel(this);
    featuresLayout->addWidget(hitDieLabel);
    
    primaryAbilityLabel = new QLabel(this);
    featuresLayout->addWidget(primaryAbilityLabel);
    
    savesLabel = new QLabel(this);
    featuresLayout->addWidget(savesLabel);
    
    contentLayout->addWidget(featuresGroup);
    
    // Proficiencies
    QGroupBox *profGroup = new QGroupBox("Владения", this);
    QVBoxLayout *profLayout = new QVBoxLayout(profGroup);
    
    armorLabel = new QLabel(this);
    armorLabel->setWordWrap(true);
    profLayout->addWidget(armorLabel);
    
    weaponsLabel = new QLabel(this);
    weaponsLabel->setWordWrap(true);
    profLayout->addWidget(weaponsLabel);
    
    contentLayout->addWidget(profGroup);

    QGroupBox *detailsGroup = new QGroupBox("Прогрессия и умения", this);
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsGroup);

    detailsText = new QTextEdit(this);
    detailsText->setReadOnly(true);
    detailsText->setMinimumHeight(260);
    detailsLayout->addWidget(detailsText);

    contentLayout->addWidget(detailsGroup);
    
    contentLayout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    // Buttons (Bottom)
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    backButton = new QPushButton("Назад", this);
    continueButton = new QPushButton("Продолжить", this);
    
    backButton->setFixedSize(120, 40);
    continueButton->setFixedSize(120, 40);
    
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(backButton);
    buttonsLayout->addWidget(continueButton);
    buttonsLayout->addStretch();
    
    mainLayout->addLayout(buttonsLayout);

    connect(backButton, &QPushButton::clicked, this, &ClassDetailsPage::backClicked);
    connect(continueButton, &QPushButton::clicked, this, &ClassDetailsPage::continueClicked);
}

void ClassDetailsPage::setClass(const Class &cls)
{
    m_class = cls;
    
    titleLabel->setText(cls.name);
    descriptionLabel->setText(cls.description);
    
    if (!cls.imagePath.isEmpty()) {
        QPixmap pix(cls.imagePath);
        if (!pix.isNull()) {
            imageLabel->setPixmap(pix.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            imageLabel->clear();
        }
    } else {
        imageLabel->clear();
    }
    
    hitDieLabel->setText(QString("<b>Кость Хитов:</b> 1d%1").arg(cls.hitDie));
    primaryAbilityLabel->setText(QString("<b>Основная характеристика:</b> %1").arg(cls.primaryAbility));
    savesLabel->setText(QString("<b>Спасброски:</b> %1").arg(cls.savingThrowProficiencies.join(", ")));
    
    armorLabel->setText(QString("<b>Доспехи:</b> %1").arg(cls.armorProficiencies.join(", ")));
    weaponsLabel->setText(QString("<b>Оружие:</b> %1").arg(cls.weaponProficiencies.join(", ")));

    QStringList details;

    if (!cls.progression.isEmpty()) {
        details << QStringLiteral("Прогрессия по уровням:");
        for (const ClassLevelProgression &entry : cls.progression) {
            details << QStringLiteral("%1 ур. (%2): %3")
                .arg(entry.level)
                .arg(signedNumber(entry.proficiencyBonus))
                .arg(entry.features.isEmpty() ? QStringLiteral("—") : entry.features.join(QStringLiteral(", ")));
        }
    }

    if (!cls.featureSections.isEmpty()) {
        if (!details.isEmpty()) {
            details << QString();
        }
        details << QStringLiteral("Классовые умения:");
        for (const ClassSection &section : cls.featureSections) {
            QString line = section.title;
            if (!section.levelText.trimmed().isEmpty()) {
                line += QStringLiteral(" — %1").arg(section.levelText.trimmed());
            }
            details << line;
        }
    }

    if (!cls.subclasses.isEmpty()) {
        if (!details.isEmpty()) {
            details << QString();
        }
        details << QStringLiteral("Архетипы:");
        for (const ClassSubclass &subclass : cls.subclasses) {
            details << subclass.name;
        }
    }

    detailsText->setPlainText(details.isEmpty()
        ? QStringLiteral("Для этого класса пока нет расширенной структуры прогрессии.")
        : details.join(QStringLiteral("\n")));
}

Class ClassDetailsPage::currentClass() const
{
    return m_class;
}
