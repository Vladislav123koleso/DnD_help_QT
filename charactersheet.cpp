#include "charactersheet.h"
#include "ui_charactersheet.h"
#include <QDebug>

CharacterSheet::CharacterSheet(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CharacterSheet),
    m_character(nullptr)
{
    ui->setupUi(this);

    // Populate Alignment Combo
    QStringList alignments = {
        "", 
        "Законно добрый", "Нейтрально добрый", "Хаотический добрый",
        "Законно нейтральный", "Истинно нейтральный", "Хаотический нейтральный",
        "Законно злой", "Нейтрально злой", "Хаотический злой"
    };
    ui->alignmentCombo->addItems(alignments);

    // Connect signals
    connect(ui->backgroundEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->alignmentCombo, &QComboBox::textActivated, this, &CharacterSheet::onFieldChanged);
    connect(ui->ageEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->heightEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->weightEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->skinEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->hairEdit, &QLineEdit::editingFinished, this, &CharacterSheet::onFieldChanged);
    connect(ui->appearanceEdit, &QTextEdit::textChanged, this, &CharacterSheet::onFieldChanged);
}

CharacterSheet::~CharacterSheet()
{
    delete ui;
}

void CharacterSheet::setCharacter(Character *c)
{
    if (m_character) {
        disconnect(m_character, nullptr, this, nullptr);
    }
    m_character = c;
    
    if (m_character) {
        updateFromCharacter();
    } else {
        ui->nameEdit->clear();
        ui->classLevelEdit->clear();
        ui->raceEdit->clear();
        // Clear others if needed
    }
}

void CharacterSheet::updateFromCharacter()
{
    if (!m_character) return;

    auto scoreWithMod = [](int score) {
        int mod = Character::abilityModifier(score);
        return QString("%1 (%2%3)")
            .arg(score)
            .arg(mod >= 0 ? "+" : "")
            .arg(mod);
    };
    
    // Header
    ui->nameEdit->setText(m_character->name());
    QString classSummary = m_character->characterClass().trimmed();
    if (classSummary.isEmpty()) {
        ui->classLevelEdit->setText(QString("Уровень %1").arg(m_character->level));
    } else {
        QString lvl = QString("%1 (общий ур. %2)").arg(classSummary).arg(m_character->level);
        ui->classLevelEdit->setText(lvl);
    }
    ui->raceEdit->setText(m_character->race());
    
    // Characteristics
    ui->strEdit->setText(scoreWithMod(m_character->strength));
    ui->dexEdit->setText(scoreWithMod(m_character->dexterity));
    ui->conEdit->setText(scoreWithMod(m_character->constitution));
    ui->intEdit->setText(scoreWithMod(m_character->intelligence));
    ui->wisEdit->setText(scoreWithMod(m_character->wisdom));
    ui->chaEdit->setText(scoreWithMod(m_character->charisma));
    
    // Combat
    ui->acEdit->setText(QString::number(m_character->armorClass));
    ui->initiativeEdit->setText(QString::number(m_character->initiative));
    ui->speedEdit->setText(QString::number(m_character->speed));
    ui->hpMaxEdit->setText(QString::number(m_character->maxHp));
    
    // Editable ones (Prevent overwriting user input while editing if possible)
    if (!ui->backgroundEdit->hasFocus()) ui->backgroundEdit->setText(m_character->background);
    if (!ui->appearanceEdit->hasFocus()) ui->appearanceEdit->setText(m_character->appearance);
    if (!ui->ageEdit->hasFocus()) ui->ageEdit->setText(m_character->age);
    if (!ui->heightEdit->hasFocus()) ui->heightEdit->setText(m_character->height);
    if (!ui->weightEdit->hasFocus()) ui->weightEdit->setText(m_character->weight);
    if (!ui->skinEdit->hasFocus()) ui->skinEdit->setText(m_character->skin);
    if (!ui->hairEdit->hasFocus()) ui->hairEdit->setText(m_character->hair);
    
    // Alignment combo set current text
    int index = ui->alignmentCombo->findText(m_character->alignment);
    if (index != -1) ui->alignmentCombo->setCurrentIndex(index);
}

void CharacterSheet::onFieldChanged()
{
    if (!m_character) return;
    
    m_character->background = ui->backgroundEdit->text();
    m_character->appearance = ui->appearanceEdit->toPlainText();
    m_character->age = ui->ageEdit->text();
    m_character->height = ui->heightEdit->text();
    m_character->weight = ui->weightEdit->text();
    m_character->skin = ui->skinEdit->text();
    m_character->hair = ui->hairEdit->text();
    m_character->alignment = ui->alignmentCombo->currentText();
}

