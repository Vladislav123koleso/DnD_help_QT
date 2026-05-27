#ifndef CLASSDETAILSPAGE_H
#define CLASSDETAILSPAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include "class.h"

class ClassDetailsPage : public QWidget {
    Q_OBJECT
public:
    explicit ClassDetailsPage(QWidget *parent = nullptr);
    void setClass(const Class &cls);
    Class currentClass() const;

signals:
    void backClicked();
    void continueClicked();

private:
   QLabel *titleLabel;
   QLabel *imageLabel;
   QLabel *descriptionLabel;
   
   QLabel *hitDieLabel;
   QLabel *primaryAbilityLabel;
   QLabel *savesLabel;
   QLabel *armorLabel;
   QLabel *weaponsLabel;
   QLabel *skillsLabel;
   QLabel *toolsLabel;
    QTextEdit *progressionText;
    QListWidget *featuresList;
    QListWidget *subclassesList;
    QTextEdit *subclassDetailsText;

   QPushButton *backButton;
   QPushButton *continueButton;
   
   Class m_class;

   void updateSubclassDetails(QListWidgetItem *item);
   void showFeaturePopup(const QString &title, const QString &description, const QPoint &globalPos);
};

#endif // CLASSDETAILSPAGE_H