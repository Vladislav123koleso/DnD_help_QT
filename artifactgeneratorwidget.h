#ifndef ARTIFACTGENERATORWIDGET_H
#define ARTIFACTGENERATORWIDGET_H

#include <QMap>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class ArtifactGeneratorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ArtifactGeneratorWidget(QWidget *parent = nullptr);

private slots:
    void generateArtifacts();
    void generateRandomArtifact();

private:
    struct RarityRule {
        QString key;
        QString display;
        QString price;
        int attunementChance = 50;
        int bonusMin = 0;
        int bonusMax = 0;
        int passiveMin = 1;
        int passiveMax = 1;
        int activeMin = 0;
        int activeMax = 0;
        int minorBeneficialMin = 0;
        int minorBeneficialMax = 0;
        int majorBeneficialMin = 0;
        int majorBeneficialMax = 0;
        int minorDetrimentalMin = 0;
        int minorDetrimentalMax = 0;
        int majorDetrimentalMin = 0;
        int majorDetrimentalMax = 0;
        int chargeChance = 50;
    };

    struct ArtifactTypeData {
        QString key;
        QString display;
        QStringList nameCores;
        QStringList nameSuffixes;
        QStringList nameFormats;
        QStringList forms;
        QStringList passivePool;
        QStringList activePool;
        QStringList attunementRequirements;
    };

    struct ThemeData {
        QString key;
        QString display;
        QStringList prefixes;
        QStringList origins;
        QStringList materials;
        QStringList passivePool;
        QStringList activePool;
        QStringList curses;
        QStringList quirks;
        QStringList destroy;
    };

    struct PurposeData {
        QString key;
        QString display;
        QStringList passivePool;
        QStringList activePool;
    };

    struct ArtifactResult {
        QString name;
        QString rarityDisplay;
        QString typeDisplay;
        QString form;
        QString attunement;
        QString price;
        QString appearance;
        QString history;
        QString charges;
        QString activationWord;
        QString destruction;
        QStringList passives;
        QStringList actives;
        QStringList minorBeneficial;
        QStringList majorBeneficial;
        QStringList minorDetrimental;
        QStringList majorDetrimental;
        QStringList quirks;
    };

    void setupUi();
    bool loadData();
    bool loadArtifactData(const QString &path);
    QString resolveDataPath(const QString &relativePath) const;

    void fillCombos();
    QString randomFrom(const QStringList &values) const;
    int rollInclusive(int minValue, int maxValue) const;
    QString applyTemplate(QString templ, const QMap<QString, QString> &tokens) const;
    QString cleanupText(QString text) const;
    QString chooseAttunement(const ArtifactTypeData &typeData,
                             const RarityRule &rarityRule,
                             const QString &modeOverride = QString()) const;
    QString buildName(const ArtifactTypeData &typeData, const ThemeData &themeData) const;
    QString buildChargesText(const RarityRule &rarityRule) const;
    QString buildAppearance(const ArtifactTypeData &typeData, const ThemeData &themeData) const;
    QString buildHistory(const ArtifactTypeData &typeData, const ThemeData &themeData) const;
    QString buildDestruction(const ThemeData &themeData) const;
    QString renderArtifact(const ArtifactResult &result, int index) const;
    QStringList takeUniqueRandom(const QStringList &pool, int count) const;
    ArtifactResult generateOneArtifact(bool fullyRandom = false) const;

    const ArtifactTypeData *selectedType() const;
    const ThemeData *selectedTheme() const;
    const PurposeData *selectedPurpose() const;
    const RarityRule *selectedRarity() const;
    const ArtifactTypeData *randomType() const;
    const ThemeData *randomTheme() const;
    const PurposeData *randomPurpose() const;
    const RarityRule *randomRarity() const;

    QMap<QString, RarityRule> m_rarities;
    QMap<QString, ArtifactTypeData> m_types;
    QMap<QString, ThemeData> m_themes;
    QMap<QString, PurposeData> m_purposes;
    QStringList m_minorBeneficial;
    QStringList m_majorBeneficial;
    QStringList m_minorDetrimental;
    QStringList m_majorDetrimental;
    QStringList m_chargeRules;
    QStringList m_activationWords;
    QStringList m_historyTemplates;
    QStringList m_appearanceTemplates;
    QString m_defaultRarityKey;

    QComboBox *typeCombo = nullptr;
    QComboBox *rarityCombo = nullptr;
    QComboBox *themeCombo = nullptr;
    QComboBox *purposeCombo = nullptr;
    QComboBox *attunementModeCombo = nullptr;
    QSpinBox *countSpin = nullptr;
    QCheckBox *drawbacksCheck = nullptr;
    QCheckBox *strictArtifactCheck = nullptr;
    QPushButton *generateBtn = nullptr;
    QPushButton *generateRandomBtn = nullptr;
    QPlainTextEdit *resultOutput = nullptr;
    QLabel *statusLabel = nullptr;
};

#endif // ARTIFACTGENERATORWIDGET_H
