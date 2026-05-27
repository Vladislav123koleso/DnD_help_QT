#ifndef CLASS_SELECTION_PAGE_H
#define CLASS_SELECTION_PAGE_H

#include <QWidget>
#include <QStackedWidget>
#include <QMap>
#include "class.h"
#include "classcard.h"
#include "classdetailspage.h"

class FlowLayout;

class ClassSelectionPage : public QWidget {
    Q_OBJECT
public:
    explicit ClassSelectionPage(QWidget *parent = nullptr);

signals:
    void classChosen(const Class &cls);

public:
    Class getClassData(const QString &name);

public slots:
    void onClassSelected(const QString &className);
    void showList();
    void confirmSelection();

    void setExcludedClassNames(const QStringList &classNames);
    void clearClassFilters();

private:
   void setupUi();
   void rebuildClassList();
   QStackedWidget *stackedWidget;
   QWidget *listPage;
   QWidget *listScrollContent = nullptr;
   FlowLayout *listContentLayout = nullptr;
   ClassDetailsPage *detailsPage;
   
   QMap<QString, Class> classData;
   QStringList excludedClassNames;
   void loadClassData();
    QString resolveClassesJsonPath() const;
    QString detectImagePath(const QString &classSlug) const;
    QString shortDescription(const Class &cls) const;
};

#endif // CLASS_SELECTION_PAGE_H