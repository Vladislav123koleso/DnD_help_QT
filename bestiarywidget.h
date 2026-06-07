#ifndef BESTIARYWIDGET_H
#define BESTIARYWIDGET_H

#include <QWidget>

#include "creature.h"

class QComboBox;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextEdit;

class BestiaryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BestiaryWidget(QWidget *parent = nullptr);

private slots:
    void filterCreatures();
    void onCreatureSelected(QListWidgetItem *item);
    void showChallengeGuideDialog();

private:
    void loadCreatures();
    void populateChallengeFilter();
    QString generateHtml(const Creature &creature);
    QString formatModifier(int score);
    QString normalizeChallengeToken(const QString &challenge) const;
    double challengeValue(const QString &token) const;
    bool matchesSelectedChallenge(const Creature &creature) const;
    QString challengeForThresholdXp(int xp) const;
    QString buildDynamicMemoText(int partySize, int level) const;
    QString buildChallengeGuideHtml() const;

    QLineEdit *searchBar = nullptr;
    QComboBox *challengeFilter = nullptr;
    QPushButton *challengeGuideBtn = nullptr;
    QListWidget *creatureList = nullptr;
    QTextEdit *detailsText = nullptr;

    QList<Creature> allCreatures;

    int guidePartySize = 4;
    int guidePartyLevel = 3;
};

#endif // BESTIARYWIDGET_H
