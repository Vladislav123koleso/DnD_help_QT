#ifndef CHARACTERSHEET_H
#define CHARACTERSHEET_H

#include <QWidget>
#include <QComboBox>
#include "character.h"

namespace Ui {
class CharacterSheet;
}

class CharacterSheet : public QWidget
{
    Q_OBJECT

public:
    explicit CharacterSheet(QWidget *parent = nullptr);
    ~CharacterSheet();

    void setCharacter(Character *c);

private slots:
    void onFieldChanged();

private:
    Ui::CharacterSheet *ui;
    Character *m_character;

    void updateFromCharacter();
};

#endif // CHARACTERSHEET_H