#ifndef BESTIARYWIDGET_H
#define BESTIARYWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QTextEdit>
#include "creature.h"

class BestiaryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BestiaryWidget(QWidget *parent = nullptr);

private slots:
    void filterCreatures();
    void onCreatureSelected(QListWidgetItem *item);

private:
    QLineEdit *searchBar;
    QListWidget *creatureList;
    QTextEdit *detailsText; 

    QList<Creature> allCreatures;
    
    void loadCreatures();
    QString generateHtml(const Creature& creature);
    QString formatModifier(int score);
};

#endif // BESTIARYWIDGET_H
