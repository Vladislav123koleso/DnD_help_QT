#ifndef RACEDETAILSPAGE_H
#define RACEDETAILSPAGE_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include "race.h"

class RaceDetailsPage : public QWidget {
    Q_OBJECT
public:
    explicit RaceDetailsPage(QWidget *parent = nullptr);
    void setRace(const Race &race);
    Race currentRace() const;

signals:
    void backClicked();
    void continueClicked();

private:
   QLabel *titleLabel;
   QLabel *imageLabel; // New
   QLabel *descriptionLabel;
   QLabel *statsLabel;
   QLabel *traitsLabel;
   QLabel *speedLabel;
   QLabel *sizeLabel;
   QLabel *languagesLabel;
    QLabel *subracesLabel;
   
   QVBoxLayout *traitsLayout; 
   QWidget *traitsContainer;

   QPushButton *backButton;
   QPushButton *continueButton;
   
   Race m_race;
};

#endif // RACEDETAILSPAGE_H