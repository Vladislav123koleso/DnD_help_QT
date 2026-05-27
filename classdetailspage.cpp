#include "classdetailspage.h"
#include <QFont>
#include <QDebug>
#include <QGroupBox>
#include <QPixmap>
#include <QSplitter>
#include <QDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QScreen>

namespace {

QString signedNumber(int value)
{
    return QString("%1%2").arg(value >= 0 ? "+" : "").arg(value);
}

QString sectionDetailsText(const ClassSection &section)
{
    QStringList lines;
    if (!section.levelText.trimmed().isEmpty()) {
        lines << section.levelText.trimmed();
    }
    if (section.optional) {
        lines << QStringLiteral("Опциональное умение");
    }
    if (!section.description.trimmed().isEmpty()) {
        lines << section.description.trimmed();
    }
    return lines.join(QStringLiteral("\n\n"));
}

QString formatSubclassDetails(const ClassSubclass &subclass)
{
    QStringList lines;
    if (!subclass.description.trimmed().isEmpty()) {
        lines << subclass.description.trimmed();
    }
    if (!subclass.sections.isEmpty()) {
        lines << QStringLiteral("Умения подкласса:");
        for (const ClassSection &section : subclass.sections) {
            QStringList sectionLines;
            sectionLines << section.title.trimmed();
            const QString details = sectionDetailsText(section);
            if (!details.trimmed().isEmpty()) {
                sectionLines << details;
            }
            lines << sectionLines.join(QStringLiteral("\n"));
        }
    }
    return lines.join(QStringLiteral("\n\n"));
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

    skillsLabel = new QLabel(this);
    skillsLabel->setWordWrap(true);
    profLayout->addWidget(skillsLabel);

    toolsLabel = new QLabel(this);
    toolsLabel->setWordWrap(true);
    profLayout->addWidget(toolsLabel);
    
    contentLayout->addWidget(profGroup);

    QGroupBox *detailsGroup = new QGroupBox("Прогрессия и умения", this);
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsGroup);

    progressionText = new QTextEdit(this);
    progressionText->setReadOnly(true);
    progressionText->setMinimumHeight(120);
    detailsLayout->addWidget(progressionText);

    QLabel *featuresHintLabel = new QLabel(
        QStringLiteral("Классовые умения (нажмите, чтобы открыть описание):"),
        detailsGroup);
    featuresHintLabel->setWordWrap(true);
    detailsLayout->addWidget(featuresHintLabel);

    featuresList = new QListWidget(this);
    featuresList->setAlternatingRowColors(true);
    featuresList->setMinimumHeight(180);
    detailsLayout->addWidget(featuresList);

    contentLayout->addWidget(detailsGroup);

    QGroupBox *subclassesGroup = new QGroupBox("Подклассы", this);
    QVBoxLayout *subclassesLayout = new QVBoxLayout(subclassesGroup);
    QLabel *subclassesHintLabel = new QLabel(
        QStringLiteral("Нажмите на подкласс слева, чтобы увидеть, какие умения он даёт по правилам D&D 5e."),
        subclassesGroup);
    subclassesHintLabel->setWordWrap(true);
    subclassesLayout->addWidget(subclassesHintLabel);

    QSplitter *subclassesSplitter = new QSplitter(Qt::Horizontal, subclassesGroup);
    subclassesList = new QListWidget(subclassesSplitter);
    subclassesList->setAlternatingRowColors(true);
    subclassDetailsText = new QTextEdit(subclassesSplitter);
    subclassDetailsText->setReadOnly(true);
    subclassDetailsText->setMinimumHeight(220);
    subclassesSplitter->setStretchFactor(0, 1);
    subclassesSplitter->setStretchFactor(1, 2);
    subclassesLayout->addWidget(subclassesSplitter);
    contentLayout->addWidget(subclassesGroup);
    
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
    connect(subclassesList, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *current) {
        updateSubclassDetails(current);
    });
    connect(featuresList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item) {
            return;
        }
        const QPoint anchor = featuresList->viewport()->mapToGlobal(featuresList->visualItemRect(item).bottomLeft());
        showFeaturePopup(
            item->text(),
            item->data(Qt::UserRole).toString(),
            anchor);
    });
}

void ClassDetailsPage::showFeaturePopup(const QString &title, const QString &description, const QPoint &globalPos)
{
    if (description.trimmed().isEmpty()) {
        return;
    }

    QDialog *popup = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setMinimumWidth(420);
    popup->setMaximumWidth(560);

    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(12, 12, 12, 12);

    QLabel *titleLabel = new QLabel(QStringLiteral("<b>%1</b>").arg(title.toHtmlEscaped()), popup);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    QTextEdit *body = new QTextEdit(popup);
    body->setReadOnly(true);
    body->setPlainText(description.trimmed());
    body->setMinimumHeight(160);
    body->setMaximumHeight(360);
    layout->addWidget(body);

    QPoint position = globalPos + QPoint(8, 8);
    if (QScreen *screen = QGuiApplication::screenAt(position)) {
        const QRect available = screen->availableGeometry();
        popup->adjustSize();
        if (position.x() + popup->width() > available.right()) {
            position.setX(available.right() - popup->width() - 8);
        }
        if (position.y() + popup->height() > available.bottom()) {
            position.setY(globalPos.y() - popup->height() - 8);
        }
    }
    popup->move(position);
    popup->show();
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
    
    armorLabel->setText(QString("<b>Доспехи:</b> %1")
                            .arg(cls.armorProficiencies.isEmpty() ? QStringLiteral("—") : cls.armorProficiencies.join(", ")));
    weaponsLabel->setText(QString("<b>Оружие:</b> %1")
                             .arg(cls.weaponProficiencies.isEmpty() ? QStringLiteral("—") : cls.weaponProficiencies.join(", ")));

    if (cls.skillChoiceCount > 0 && !cls.skillChoices.isEmpty()) {
        skillsLabel->setText(QString("<b>Навыки:</b> выберите %1 из %2")
                                 .arg(cls.skillChoiceCount)
                                 .arg(cls.skillChoices.join(", ")));
        skillsLabel->show();
    } else {
        skillsLabel->setText(QStringLiteral("<b>Навыки:</b> —"));
        skillsLabel->show();
    }

    if (!cls.toolProficiencies.isEmpty()) {
        toolsLabel->setText(QString("<b>Инструменты:</b> %1").arg(cls.toolProficiencies.join(", ")));
        toolsLabel->show();
    } else {
        toolsLabel->hide();
    }

    QStringList progressionLines;
    if (!cls.progression.isEmpty()) {
        progressionLines << QStringLiteral("Прогрессия по уровням:");
        for (const ClassLevelProgression &entry : cls.progression) {
            progressionLines << QStringLiteral("%1 ур. (%2): %3")
                                  .arg(entry.level)
                                  .arg(signedNumber(entry.proficiencyBonus))
                                  .arg(entry.features.isEmpty() ? QStringLiteral("—") : entry.features.join(QStringLiteral(", ")));
        }
    }
    progressionText->setPlainText(progressionLines.isEmpty()
        ? QStringLiteral("Для этого класса пока нет таблицы прогрессии.")
        : progressionLines.join(QStringLiteral("\n")));

    featuresList->clear();
    if (cls.featureSections.isEmpty()) {
        QListWidgetItem *emptyItem = new QListWidgetItem(
            QStringLiteral("Описания умений для этого класса пока не загружены."),
            featuresList);
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
    } else {
        for (const ClassSection &section : cls.featureSections) {
            QString label = section.title.trimmed();
            if (!section.levelText.trimmed().isEmpty()) {
                label += QStringLiteral(" — %1").arg(section.levelText.trimmed());
            }
            QListWidgetItem *item = new QListWidgetItem(label, featuresList);
            item->setData(Qt::UserRole, sectionDetailsText(section));
            item->setToolTip(QStringLiteral("Нажмите, чтобы прочитать описание"));
        }
    }

    subclassesList->clear();
    if (cls.subclasses.isEmpty()) {
        subclassDetailsText->setPlainText(QStringLiteral("Для этого класса в загруженных данных нет подклассов."));
    } else {
        for (const ClassSubclass &subclass : cls.subclasses) {
            QListWidgetItem *item = new QListWidgetItem(subclass.name, subclassesList);
            item->setData(Qt::UserRole, formatSubclassDetails(subclass));
        }
        subclassesList->setCurrentRow(0);
    }
}

Class ClassDetailsPage::currentClass() const
{
    return m_class;
}

void ClassDetailsPage::updateSubclassDetails(QListWidgetItem *item)
{
    if (!item) {
        subclassDetailsText->setPlainText(QStringLiteral("Выберите подкласс, чтобы увидеть описание."));
        return;
    }

    subclassDetailsText->setPlainText(item->data(Qt::UserRole).toString());
}
