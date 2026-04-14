#include "racedetailspage.h"
#include <QFont>
#include <QDebug>
#include <QGroupBox>
#include <QPixmap>

RaceDetailsPage::RaceDetailsPage(QWidget *parent) : QWidget(parent)
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
    imageLabel->setFixedHeight(200); // Fixed height for image
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
    
    // Stat Increases
    QGroupBox *statsGroup = new QGroupBox("Увеличение характеристик", this);
    QVBoxLayout *statsLayout = new QVBoxLayout(statsGroup);
    statsLabel = new QLabel(this);
    statsLabel->setWordWrap(true);
    statsLayout->addWidget(statsLabel);
    contentLayout->addWidget(statsGroup);
    
    // Physical
    QGroupBox *physicalGroup = new QGroupBox("Физические данные", this);
    QVBoxLayout *physicalLayout = new QVBoxLayout(physicalGroup);
    sizeLabel = new QLabel(this);
    speedLabel = new QLabel(this);
    physicalLayout->addWidget(sizeLabel);
    physicalLayout->addWidget(speedLabel);
    contentLayout->addWidget(physicalGroup);
    
    // Traits
    QGroupBox *traitsGroup = new QGroupBox("Особенности и навыки", this);
    QVBoxLayout *traitsGroupLayout = new QVBoxLayout(traitsGroup);
    
    traitsContainer = new QWidget(this);
    traitsLayout = new QVBoxLayout(traitsContainer);
    traitsLayout->setContentsMargins(0,0,0,0);
    traitsGroupLayout->addWidget(traitsContainer);
    contentLayout->addWidget(traitsGroup);

    // Languages
    QGroupBox *langGroup = new QGroupBox("Языки", this);
    QVBoxLayout *langLayout = new QVBoxLayout(langGroup);
    languagesLabel = new QLabel(this);
    languagesLabel->setWordWrap(true);
    langLayout->addWidget(languagesLabel);
    contentLayout->addWidget(langGroup);

    // Subraces
    QGroupBox *subracesGroup = new QGroupBox("Подрасы", this);
    QVBoxLayout *subracesLayout = new QVBoxLayout(subracesGroup);
    subracesLabel = new QLabel(this);
    subracesLabel->setWordWrap(true);
    subracesLayout->addWidget(subracesLabel);
    contentLayout->addWidget(subracesGroup);
    
    contentLayout->addStretch();
    
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    // Buttons (Bottom)
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    backButton = new QPushButton("Назад", this);
    continueButton = new QPushButton("Продолжить", this);
    
    // Style buttons
    backButton->setFixedSize(120, 40);
    continueButton->setFixedSize(120, 40);
    
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(backButton);
    buttonsLayout->addWidget(continueButton);
    buttonsLayout->addStretch();
    
    mainLayout->addLayout(buttonsLayout);

    connect(backButton, &QPushButton::clicked, this, &RaceDetailsPage::backClicked);
    connect(continueButton, &QPushButton::clicked, this, &RaceDetailsPage::continueClicked);
}

void RaceDetailsPage::setRace(const Race &race)
{
    m_race = race;
    
    titleLabel->setText(race.name);
    descriptionLabel->setText(race.description);
    
    // Set Image
    if (!race.imagePath.isEmpty()) {
        QPixmap pix(race.imagePath);
        if (!pix.isNull()) {
            imageLabel->setPixmap(pix.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            imageLabel->clear();
        }
    } else {
        imageLabel->clear();
    }
    
    // Stats
    QStringList statList;
    for(auto it = race.abilityScoreIncrease.begin(); it != race.abilityScoreIncrease.end(); ++it) {
        QString statName;
        // Translate or use key directly. Assuming keys like "Dexterity", "Wisdom"
        if(it.key() == "Strength") statName = "Сила";
        else if(it.key() == "Dexterity") statName = "Ловкость";
        else if(it.key() == "Constitution") statName = "Телосложение";
        else if(it.key() == "Intelligence") statName = "Интеллект";
        else if(it.key() == "Wisdom") statName = "Мудрость";
        else if(it.key() == "Charisma") statName = "Харизма";
        else statName = it.key();
        
        statList.append(QString("%1 +%2").arg(statName).arg(it.value()));
    }
    statsLabel->setText(statList.join(", "));
    
    // Physical
    sizeLabel->setText(QString("<b>Размер:</b> %1").arg(race.size));
    QString speedText = QString("<b>Скорость:</b> %1 футов").arg(race.speed);
    if(race.flyingSpeed > 0) {
        speedText += QString("<br><b>Скорость полета:</b> %1 футов (примечание: для полета нельзя носить средний и тяжелый доспех)").arg(race.flyingSpeed);
    }
    speedLabel->setText(speedText);
    
    // Traits - Clear previous labels
    QLayoutItem *child;
    while ((child = traitsLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    
    for(auto it = race.traits.begin(); it != race.traits.end(); ++it) {
        QLabel *traitLbl = new QLabel(QString("<b>%1</b>: %2").arg(it.key(), it.value()));
        traitLbl->setWordWrap(true);
        traitsLayout->addWidget(traitLbl);
    }
    
    // Languages
    languagesLabel->setText(race.languages.join(", "));

    if (race.subraces.isEmpty()) {
        subracesLabel->setText("Нет явных подрас в загруженных данных.");
    } else {
        subracesLabel->setText(race.subraces.join(", "));
    }
}

Race RaceDetailsPage::currentRace() const
{
    return m_race;
}
