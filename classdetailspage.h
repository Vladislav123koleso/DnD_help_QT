#ifndef CLASSDETAILSPAGE_H
#define CLASSDETAILSPAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
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
    QTextEdit *detailsText;

   QPushButton *backButton;
   QPushButton *continueButton;
   
   Class m_class;
};

#endif // CLASSDETAILSPAGE_H