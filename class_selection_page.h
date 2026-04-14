#ifndef CLASS_SELECTION_PAGE_H
#define CLASS_SELECTION_PAGE_H

#include <QWidget>
#include <QStackedWidget>
#include <QMap>
#include "class.h"
#include "classcard.h"
#include "classdetailspage.h"

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

private:
   void setupUi();
   QStackedWidget *stackedWidget;
   QWidget *listPage;
   ClassDetailsPage *detailsPage;
   
   QMap<QString, Class> classData;
   void loadClassData();
};

#endif // CLASS_SELECTION_PAGE_H