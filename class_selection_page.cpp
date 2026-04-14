#include "class_selection_page.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QDebug>
#include "flowlayout.h"

ClassSelectionPage::ClassSelectionPage(QWidget *parent) : QWidget(parent) {
    loadClassData();
    setupUi();
}

void ClassSelectionPage::loadClassData()
{
    QString basePath = "D:/repos/qt/DnD_help/DndHelperDesign/DndHelperDesignContent/images/class/";

    // Fighter (Воин)
    Class fighter;
    fighter.name = "Воин";
    fighter.description = "Мастер боевых искусств, владеющий разнообразным оружием и доспехами.";
    fighter.hitDie = 10;
    fighter.primaryAbility = "Сила или Ловкость";
    fighter.savingThrowProficiencies << "Сила" << "Телосложение";
    fighter.armorProficiencies << "Все доспехи" << "Щиты";
    fighter.weaponProficiencies << "Простое оружие" << "Воинское оружие";
    fighter.imagePath = basePath + "fighter.jpg";
    classData["Воин"] = fighter;

    // Rogue (Плут)
    Class rogue;
    rogue.name = "Плут";
    rogue.description = "Преступник, использующий скрытность и хитрость для преодоления препятствий и врагов.";
    rogue.hitDie = 8;
    rogue.primaryAbility = "Ловкость";
    rogue.savingThrowProficiencies << "Ловкость" << "Интеллект";
    rogue.armorProficiencies << "Легкие доспехи";
    rogue.weaponProficiencies << "Простое оружие" << "Ручные арбалеты" << "Длинные мечи" << "Рапиры" << "Короткие мечи";
    rogue.imagePath = basePath + "rogue.jpg";
    classData["Плут"] = rogue;
    
    // Wizard (Волшебник)
    Class wizard;
    wizard.name = "Волшебник";
    wizard.description = "Ученый маг, способный изменять структуру реальности.";
    wizard.hitDie = 6;
    wizard.primaryAbility = "Интеллект";
    wizard.savingThrowProficiencies << "Интеллект" << "Мудрость";
    wizard.armorProficiencies << "Нет";
    wizard.weaponProficiencies << "Боевые посохи" << "Дротики" << "Пращи" << "Кинжалы" << "Легкие арбалеты";
    wizard.imagePath = basePath + "wizard.jpg";
    classData["Волшебник"] = wizard;
}

void ClassSelectionPage::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    stackedWidget = new QStackedWidget(this);
    
    // Page 1: List
    listPage = new QWidget();
    QVBoxLayout *listLayout = new QVBoxLayout(listPage);
    
    QScrollArea *scrollArea = new QScrollArea(listPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget *scrollContent = new QWidget();
    FlowLayout *contentLayout = new FlowLayout(scrollContent, 20, 20, 20);

    QString basePath = "D:/repos/qt/DnD_help/DndHelperDesign/DndHelperDesignContent/images/class/";
    
    // Fighter
    ClassCard *fighterCard = new ClassCard("Воин", basePath + "fighter.jpg", basePath + "fighter_hover.jpg", "Мастер оружия", scrollContent);
    connect(fighterCard, &ClassCard::classSelected, this, &ClassSelectionPage::onClassSelected);
    contentLayout->addWidget(fighterCard);

    // Rogue
    ClassCard *rogueCard = new ClassCard("Плут", basePath + "rogue.jpg", basePath + "rogue_hover.jpg", "Мастер скрытности", scrollContent);
    connect(rogueCard, &ClassCard::classSelected, this, &ClassSelectionPage::onClassSelected);
    contentLayout->addWidget(rogueCard);
    
    // Wizard
    ClassCard *wizardCard = new ClassCard("Волшебник", basePath + "wizard.jpg", basePath + "wizard_hover.jpg", "Мастер магии", scrollContent);
    connect(wizardCard, &ClassCard::classSelected, this, &ClassSelectionPage::onClassSelected);
    contentLayout->addWidget(wizardCard);

    scrollArea->setWidget(scrollContent);
    listLayout->addWidget(scrollArea);
    
    // Page 2: Details
    detailsPage = new ClassDetailsPage();
    connect(detailsPage, &ClassDetailsPage::backClicked, this, &ClassSelectionPage::showList);
    connect(detailsPage, &ClassDetailsPage::continueClicked, this, &ClassSelectionPage::confirmSelection);

    stackedWidget->addWidget(listPage);
    stackedWidget->addWidget(detailsPage);
    
    mainLayout->addWidget(stackedWidget);
}

void ClassSelectionPage::onClassSelected(const QString &className)
{
    if (classData.contains(className)) {
        detailsPage->setClass(classData[className]);
        stackedWidget->setCurrentWidget(detailsPage);
    } else {
        qDebug() << "Class data not found for:" << className;
    }
}

void ClassSelectionPage::showList()
{
    stackedWidget->setCurrentWidget(listPage);
}

void ClassSelectionPage::confirmSelection()
{
    emit classChosen(detailsPage->currentClass());
}

Class ClassSelectionPage::getClassData(const QString &name)
{
    if (classData.contains(name)) {
        return classData[name];
    }
    return Class();
}
