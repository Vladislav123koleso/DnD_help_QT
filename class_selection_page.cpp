#include "class_selection_page.h"
#include <QVBoxLayout>
#include <QScrollArea>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include "flowlayout.h"

ClassSelectionPage::ClassSelectionPage(QWidget *parent) : QWidget(parent) {
    loadClassData();
    setupUi();
}

void ClassSelectionPage::loadClassData()
{
    classData.clear();

    const QString jsonPath = resolveClassesJsonPath();
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Не удалось открыть classes_dndsu.json:" << jsonPath;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        qDebug() << "Некорректный формат classes_dndsu.json";
        return;
    }

    const QJsonArray classesArray = doc.array();
    for (const QJsonValue &value : classesArray) {
        Class cls = Class::fromJson(value.toObject());
        cls.imagePath = detectImagePath(cls.slug);

        if (!cls.name.isEmpty()) {
            classData.insert(cls.name, cls);
        }
    }
}

QString ClassSelectionPage::resolveClassesJsonPath() const
{
    QStringList candidates;
    candidates << "classes_dndsu.json";
    candidates << QDir::current().filePath("classes_dndsu.json");

    QDir appDir(QCoreApplication::applicationDirPath());
    candidates << appDir.filePath("classes_dndsu.json");

    QDir dir = appDir;
    for (int i = 0; i < 6; ++i) {
        candidates << dir.filePath("classes_dndsu.json");
        if (!dir.cdUp()) {
            break;
        }
    }

    for (const QString &candidate : candidates) {
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return "classes_dndsu.json";
}

QString ClassSelectionPage::detectImagePath(const QString &classSlug) const
{
    QString basePath;
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (dir.exists("DndHelperDesign/DndHelperDesignContent/images/class")) {
            basePath = dir.filePath("DndHelperDesign/DndHelperDesignContent/images/class/");
            break;
        }
        if (!dir.cdUp()) {
            break;
        }
    }

    if (basePath.isEmpty() || classSlug.trimmed().isEmpty()) {
        return QString();
    }

    const QStringList extensions = {"jpg", "png", "webp", "jpeg"};
    for (const QString &extension : extensions) {
        const QString candidate = basePath + classSlug + "." + extension;
        if (QFile::exists(candidate)) {
            return candidate;
        }
    }

    return QString();
}

QString ClassSelectionPage::shortDescription(const Class &cls) const
{
    QString text = cls.description.simplified();
    if (text.length() > 90) {
        text = text.left(87) + "...";
    }
    if (text.isEmpty()) {
        text = QStringLiteral("Описание отсутствует");
    }
    return text;
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

    const QStringList classNames = classData.keys();
    if (classNames.isEmpty()) {
        QLabel *emptyLabel = new QLabel("Список классов не загружен.", scrollContent);
        emptyLabel->setAlignment(Qt::AlignCenter);
        contentLayout->addWidget(emptyLabel);
    } else {
        for (const QString &className : classNames) {
            const Class cls = classData.value(className);

            QString hoverImagePath = cls.imagePath;
            if (!cls.imagePath.isEmpty()) {
                const QFileInfo info(cls.imagePath);
                const QString hoverCandidate = info.path() + "/" + info.completeBaseName() + "_hover." + info.suffix();
                if (QFile::exists(hoverCandidate)) {
                    hoverImagePath = hoverCandidate;
                }
            }

            ClassCard *card = new ClassCard(className, cls.imagePath, hoverImagePath, shortDescription(cls), scrollContent);
            connect(card, &ClassCard::classSelected, this, &ClassSelectionPage::onClassSelected);
            contentLayout->addWidget(card);
        }
    }

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
