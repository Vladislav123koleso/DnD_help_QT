#ifndef NAMEGENERATORWIDGET_H
#define NAMEGENERATORWIDGET_H

#include <QMap>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

class NameGeneratorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NameGeneratorWidget(QWidget *parent = nullptr);

private slots:
    void generateCharacterNames();
    void generateLocationNames();
    void onPlaceTypeChanged();

private:
    struct NamePool {
        QStringList full;
        QStringList prefixes;
        QStringList middles;
        QStringList suffixes;
    };

    struct RaceNameData {
        QString displayName;
        QMap<QString, NamePool> poolsByGender;
        QStringList surnames;
        QStringList nicknames;
        QStringList honorifics;
        QStringList origins;
        QStringList formats;
        double surnameChance = 0.8;
        double nicknameChance = 0.25;
        double honorificChance = 0.1;
    };

    struct PlaceNameData {
        QString displayName;
        QStringList prefixes;
        QStringList cores;
        QStringList suffixes;
        QStringList adjectives;
        QStringList nouns;
        QStringList genitives;
        QStringList formats;
    };

    void setupUi();
    bool loadData();
    bool loadNameData(const QString &path);
    bool loadPlaceData(const QString &path);
    QString resolveDataPath(const QString &relativePath) const;

    void fillRaceCombo();
    void fillTerrainCombo();

    QString randomFrom(const QStringList &values) const;
    QString composeFromPool(const NamePool &pool) const;
    QString chooseFirstName(const RaceNameData &raceData, const QString &genderKey) const;
    QString generateCharacterName(const RaceNameData &raceData, const QString &genderKey) const;

    QString buildCompound(const PlaceNameData &data) const;
    QString generatePlaceName(const PlaceNameData &data) const;

    QString applyTemplate(QString templ, const QMap<QString, QString> &tokens) const;
    QString cleanupGeneratedText(QString text) const;

    const RaceNameData *selectedRaceData() const;
    const PlaceNameData *selectedPlaceData() const;

    QMap<QString, RaceNameData> m_raceData;
    QMap<QString, PlaceNameData> m_settlementData;
    QMap<QString, PlaceNameData> m_terrainData;
    QString m_fallbackRaceKey;

    QComboBox *genderCombo = nullptr;
    QComboBox *raceCombo = nullptr;
    QSpinBox *nameCountSpin = nullptr;
    QPushButton *generateNamesBtn = nullptr;
    QPlainTextEdit *namesOutput = nullptr;

    QComboBox *placeTypeCombo = nullptr;
    QLabel *terrainTypeLabel = nullptr;
    QComboBox *terrainTypeCombo = nullptr;
    QSpinBox *placeCountSpin = nullptr;
    QPushButton *generatePlacesBtn = nullptr;
    QPlainTextEdit *placesOutput = nullptr;

    QLabel *statusLabel = nullptr;
};

#endif // NAMEGENERATORWIDGET_H
