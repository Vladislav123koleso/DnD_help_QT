#include "npccreatorwidget.h"

#include "character.h"
#include "databasemanager.h"
#include "spellselectdialog.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QColor>
#include <QDialog>
#include <QDir>
#include <QDropEvent>
#include <QFrame>
#include <QGridLayout>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QSpinBox>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QUuid>
#include <QSet>

#include <algorithm>

class NpcTreeWidget : public QTreeWidget
{
public:
    explicit NpcTreeWidget(NpcCreatorWidget *owner, QWidget *parent = nullptr)
        : QTreeWidget(parent)
        , m_owner(owner)
    {
    }

protected:
    void dropEvent(QDropEvent *event) override
    {
        QTreeWidget::dropEvent(event);
        if (m_owner && event->isAccepted()) {
            m_owner->syncStructureFromTree();
        }
    }

private:
    NpcCreatorWidget *m_owner = nullptr;
};

namespace {

QString sanitizeStorageKey(QString key)
{
    key = key.trimmed();
    if (key.isEmpty()) {
        key = QStringLiteral("default");
    }
    const QString forbidden = QStringLiteral("<>:\"/\\|?*");
    for (QChar &ch : key) {
        if (forbidden.contains(ch)) {
            ch = QLatin1Char('_');
        }
    }
    return key;
}

QString signedValue(int value)
{
    return value >= 0 ? QStringLiteral("+%1").arg(value) : QString::number(value);
}

QString normalizeToken(QString text)
{
    text = text.trimmed().toLower();
    text.replace(QChar(0x0451), QChar(0x0435)); // ё -> е
    text.remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]")));
    return text;
}

QString abilityKeyForText(const QString &text)
{
    const QString token = normalizeToken(text);
    if (token.startsWith(QStringLiteral("сил"))) {
        return QStringLiteral("str");
    }
    if (token.startsWith(QStringLiteral("лов"))) {
        return QStringLiteral("dex");
    }
    if (token.startsWith(QStringLiteral("тел")) || token.startsWith(QStringLiteral("кон"))) {
        return QStringLiteral("con");
    }
    if (token.startsWith(QStringLiteral("инт"))) {
        return QStringLiteral("int");
    }
    if (token.startsWith(QStringLiteral("мдр")) || token.startsWith(QStringLiteral("муд"))) {
        return QStringLiteral("wis");
    }
    if (token.startsWith(QStringLiteral("хар"))) {
        return QStringLiteral("cha");
    }
    return {};
}

int abilityScoreByKey(const Character *character, const QString &key)
{
    if (!character) {
        return 10;
    }
    if (key == QStringLiteral("str")) {
        return character->strength;
    }
    if (key == QStringLiteral("dex")) {
        return character->dexterity;
    }
    if (key == QStringLiteral("con")) {
        return character->constitution;
    }
    if (key == QStringLiteral("int")) {
        return character->intelligence;
    }
    if (key == QStringLiteral("wis")) {
        return character->wisdom;
    }
    if (key == QStringLiteral("cha")) {
        return character->charisma;
    }
    return 10;
}

QString abilityShortLabel(const QString &key)
{
    if (key == QStringLiteral("str")) {
        return QStringLiteral("СИЛ");
    }
    if (key == QStringLiteral("dex")) {
        return QStringLiteral("ЛОВ");
    }
    if (key == QStringLiteral("con")) {
        return QStringLiteral("ТЕЛ");
    }
    if (key == QStringLiteral("int")) {
        return QStringLiteral("ИНТ");
    }
    if (key == QStringLiteral("wis")) {
        return QStringLiteral("МДР");
    }
    if (key == QStringLiteral("cha")) {
        return QStringLiteral("ХАР");
    }
    return QStringLiteral("?");
}

QStringList splitCsv(QString text)
{
    text.replace(QLatin1Char('\n'), QLatin1Char(','));
    QStringList values;
    for (const QString &part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            values << trimmed;
        }
    }
    return values;
}

bool spellLevelKeyLess(const QString &a, const QString &b)
{
    return a.toInt() < b.toInt();
}

int armorBaseFromCombo(const QComboBox *combo)
{
    return combo ? combo->currentData(Qt::UserRole).toInt() : 10;
}

int armorDexCapFromCombo(const QComboBox *combo)
{
    return combo ? combo->currentData(Qt::UserRole + 1).toInt() : -1;
}

int shieldBonusFromCombo(const QComboBox *combo)
{
    return combo ? combo->currentData(Qt::UserRole).toInt() : 0;
}

QString stripHtml(QString text)
{
    text.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QString());
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return text;
}

QString normalizeForSearch(QString text)
{
    text = stripHtml(text).toLower();
    text.replace(QChar(0x0451), QChar(0x0435)); // ё -> е
    return text;
}

bool parseArmorAcInfo(const Item &item, bool isShield, int *baseAc, int *dexCap, bool *usesDex, int *shieldBonus)
{
    if (!baseAc || !dexCap || !usesDex || !shieldBonus) {
        return false;
    }

    *baseAc = 10;
    *dexCap = -1;
    *usesDex = false;
    *shieldBonus = 0;

    const QString merged = normalizeForSearch(item.type + QStringLiteral(" ") + item.name + QStringLiteral(" ") + item.description);
    if (!merged.contains(QStringLiteral("кд"))) {
        return false;
    }

    if (isShield) {
        QRegularExpression bonusRe(QStringLiteral("кд\\s*:\\s*\\+\\s*(\\d+)"));
        QRegularExpressionMatch bonusMatch = bonusRe.match(merged);
        if (bonusMatch.hasMatch()) {
            *shieldBonus = bonusMatch.captured(1).toInt();
            return true;
        }
        return false;
    }

    QRegularExpression baseRe(QStringLiteral("кд\\s*:\\s*(\\d+)"));
    QRegularExpressionMatch baseMatch = baseRe.match(merged);
    if (!baseMatch.hasMatch()) {
        return false;
    }

    *baseAc = baseMatch.captured(1).toInt();
    *usesDex = merged.contains(QStringLiteral("модификатор лов"));
    if (*usesDex) {
        QRegularExpression capRe(QStringLiteral("макс\\.?\\s*\\+?\\s*(\\d+)"));
        QRegularExpressionMatch capMatch = capRe.match(merged);
        *dexCap = capMatch.hasMatch() ? capMatch.captured(1).toInt() : -1;
    } else {
        *dexCap = 0;
    }

    return true;
}

QString armorDisplayText(const QString &name, int baseAc, int dexCap, bool usesDex)
{
    if (usesDex) {
        if (dexCap > -1) {
            return QStringLiteral("%1 (%2 + ЛОВ, макс. +%3)").arg(name).arg(baseAc).arg(dexCap);
        }
        return QStringLiteral("%1 (%2 + ЛОВ)").arg(name).arg(baseAc);
    }
    return QStringLiteral("%1 (%2)").arg(name).arg(baseAc);
}

QSpinBox *makeAbilitySpin(QWidget *parent)
{
    auto *spin = new QSpinBox(parent);
    spin->setRange(1, 30);
    spin->setValue(10);
    return spin;
}

void showDetailPopup(QWidget *anchorWidget, const QString &title, const QString &description, const QPoint &globalPos)
{
    auto *popup = new QDialog(anchorWidget, Qt::Popup | Qt::FramelessWindowHint);
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setMinimumWidth(420);
    popup->setMaximumWidth(560);

    auto *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *titleLabel = new QLabel(QStringLiteral("<b>%1</b>").arg(title.toHtmlEscaped()), popup);
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    auto *body = new QTextEdit(popup);
    body->setReadOnly(true);
    body->setPlainText(description.trimmed().isEmpty()
                           ? QStringLiteral("Описание отсутствует.")
                           : description.trimmed());
    body->setMinimumHeight(160);
    body->setMaximumHeight(360);
    layout->addWidget(body);

    QPoint position = globalPos + QPoint(8, 8);
    if (QScreen *screen = QGuiApplication::screenAt(position)) {
        const QRect available = screen->availableGeometry();
        popup->adjustSize();
        if (position.x() + popup->width() > available.right()) {
            position.setX(available.right() - popup->width() - 8);
        }
        if (position.y() + popup->height() > available.bottom()) {
            position.setY(globalPos.y() - popup->height() - 8);
        }
    }
    popup->move(position);
    popup->show();
}

} // namespace

NpcCreatorWidget::NpcCreatorWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("NpcCreatorWidget"));
    creationService = new CharacterCreationService(this);
    setupUi();

    saveTimer = new QTimer(this);
    saveTimer->setSingleShot(true);
    saveTimer->setInterval(800);
    connect(saveTimer, &QTimer::timeout, this, &NpcCreatorWidget::persistToDisk);

    if (QCoreApplication *app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() { persistToDisk(); });
    }

    ensureStorageReady();
}

NpcCreatorWidget::~NpcCreatorWidget()
{
    if (saveTimer) {
        saveTimer->stop();
    }
    persistToDisk();
}

void NpcCreatorWidget::setStorageScope(const QString &scope)
{
    const QString next = sanitizeStorageKey(scope);
    if (next == m_storageScope) {
        return;
    }

    saveCurrentNpcFromForm();
    if (saveTimer) {
        saveTimer->stop();
    }
    persistToDisk();

    m_storageScope = next;
    npcs.clear();
    folders.clear();
    currentNpcIdValue.clear();
    if (workCharacter) {
        workCharacter->deleteLater();
        workCharacter = nullptr;
    }
    loadFromDisk();
    rebuildTree();
    setFormEnabled(false);
    updateToolbarState();
}

QString NpcCreatorWidget::storageFilePath() const
{
    if (m_storageScope.isEmpty()) {
        return {};
    }
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(base)) {
        return {};
    }
    return QDir(base).filePath(QStringLiteral("npcs_%1.json").arg(m_storageScope));
}

void NpcCreatorWidget::loadFromDisk()
{
    m_loading = true;
    npcs.clear();
    folders.clear();
    currentNpcIdValue.clear();

    const NpcStorageData data = NpcStorageData::loadFromFile(storageFilePath());
    folders = data.folders;
    npcs = data.npcs;

    m_loading = false;
}

void NpcCreatorWidget::persistToDisk()
{
    if (m_storageScope.isEmpty()) {
        return;
    }

    saveCurrentNpcFromForm();

    const QString path = storageFilePath();
    if (path.isEmpty()) {
        return;
    }

    QList<NpcFolder> folderList = folders.values();
    std::sort(folderList.begin(), folderList.end(), [](const NpcFolder &a, const NpcFolder &b) {
        if (a.sortOrder != b.sortOrder) {
            return a.sortOrder < b.sortOrder;
        }
        return a.name.localeAwareCompare(b.name) < 0;
    });

    QList<NpcEntry> npcList = npcs.values();
    std::sort(npcList.begin(), npcList.end(), [](const NpcEntry &a, const NpcEntry &b) {
        if (a.sortOrder != b.sortOrder) {
            return a.sortOrder < b.sortOrder;
        }
        return a.listName.localeAwareCompare(b.listName) < 0;
    });

    QJsonArray foldersArray;
    for (const NpcFolder &folder : folderList) {
        foldersArray.append(folder.toJson());
    }

    QJsonArray npcsArray;
    for (const NpcEntry &entry : npcList) {
        npcsArray.append(entry.toJson());
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("folders"), foldersArray);
    root.insert(QStringLiteral("npcs"), npcsArray);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "NpcCreatorWidget: cannot write" << path << file.errorString();
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void NpcCreatorWidget::schedulePersist()
{
    if (!m_loading && saveTimer) {
        saveTimer->start();
    }
}

void NpcCreatorWidget::ensureWorkCharacter()
{
    bool created = false;
    if (!workCharacter) {
        workCharacter = new Character(this);
        created = true;
    }
    const QString boundNpcId = workCharacter->property("npcId").toString();
    if ((created || boundNpcId != currentNpcIdValue) && !currentNpcIdValue.isEmpty() && npcs.contains(currentNpcIdValue)) {
        syncCharacterFromNpcEntry(npcs.value(currentNpcIdValue), *workCharacter);
        workCharacter->setProperty("npcId", currentNpcIdValue);
    }
    workCharacter->setName(nameEdit ? nameEdit->text().trimmed() : QString());
    workCharacter->alignment = alignmentCombo ? alignmentCombo->currentText().trimmed() : QString();
    creationService->setParentWidget(this);
    creationService->setCharacter(workCharacter);
    creationService->resetForNpc();
}

void NpcCreatorWidget::syncWorkCharacterAbilityScores()
{
    ensureWorkCharacter();

    workCharacter->strength = strSpin->value();
    workCharacter->dexterity = dexSpin->value();
    workCharacter->constitution = conSpin->value();
    workCharacter->intelligence = intSpin->value();
    workCharacter->wisdom = wisSpin->value();
    workCharacter->charisma = chaSpin->value();

    QMap<QString, int> scores;
    scores.insert(QStringLiteral("Сила"), workCharacter->strength);
    scores.insert(QStringLiteral("Ловкость"), workCharacter->dexterity);
    scores.insert(QStringLiteral("Телосложение"), workCharacter->constitution);
    scores.insert(QStringLiteral("Интеллект"), workCharacter->intelligence);
    scores.insert(QStringLiteral("Мудрость"), workCharacter->wisdom);
    scores.insert(QStringLiteral("Харизма"), workCharacter->charisma);
    creationService->setBaseAbilityScores(scores);
    creationService->setTargetLevel(levelSpin ? levelSpin->value() : workCharacter->level);
}

void NpcCreatorWidget::applyFormValuesToWorkCharacter()
{
    ensureWorkCharacter();
    NpcEntry draftEntry = readNpcFromForm();
    syncCharacterFromNpcEntry(draftEntry, *workCharacter);
    workCharacter->setName(draftEntry.name.trimmed().isEmpty() ? draftEntry.listName : draftEntry.name);
    workCharacter->level = qBound(1, draftEntry.level, 20);
    workCharacter->recalculateDerivedStats(false);
}

void NpcCreatorWidget::updateAbilityModifiersLabel()
{
    if (!strSpin || !dexSpin || !conSpin || !intSpin || !wisSpin || !chaSpin) {
        return;
    }

    const QString strMod = signedValue(Character::abilityModifier(strSpin->value()));
    const QString dexMod = signedValue(Character::abilityModifier(dexSpin->value()));
    const QString conMod = signedValue(Character::abilityModifier(conSpin->value()));
    const QString intMod = signedValue(Character::abilityModifier(intSpin->value()));
    const QString wisMod = signedValue(Character::abilityModifier(wisSpin->value()));
    const QString chaMod = signedValue(Character::abilityModifier(chaSpin->value()));

    if (strAbilityLabel) {
        strAbilityLabel->setText(QStringLiteral("СИЛ (%1)").arg(strMod));
    }
    if (dexAbilityLabel) {
        dexAbilityLabel->setText(QStringLiteral("ЛОВ (%1)").arg(dexMod));
    }
    if (conAbilityLabel) {
        conAbilityLabel->setText(QStringLiteral("ТЕЛ (%1)").arg(conMod));
    }
    if (intAbilityLabel) {
        intAbilityLabel->setText(QStringLiteral("ИНТ (%1)").arg(intMod));
    }
    if (wisAbilityLabel) {
        wisAbilityLabel->setText(QStringLiteral("МДР (%1)").arg(wisMod));
    }
    if (chaAbilityLabel) {
        chaAbilityLabel->setText(QStringLiteral("ХАР (%1)").arg(chaMod));
    }
}

void NpcCreatorWidget::reloadArmorSourcesFromItems()
{
    if (!armorSourceCombo || !shieldSourceCombo) {
        return;
    }

    const QString previousArmor = armorSourceCombo->currentText();
    const QString previousShield = shieldSourceCombo->currentText();
    QSignalBlocker armorBlocker(armorSourceCombo);
    QSignalBlocker shieldBlocker(shieldSourceCombo);

    armorSourceCombo->clear();
    shieldSourceCombo->clear();

    armorSourceCombo->addItem(QStringLiteral("Без доспеха"));
    armorSourceCombo->setItemData(0, 10, Qt::UserRole);
    armorSourceCombo->setItemData(0, -1, Qt::UserRole + 1);
    shieldSourceCombo->addItem(QStringLiteral("Без щита"), 0);

    QSet<QString> usedArmorNames;
    QSet<QString> usedShieldNames;

    const QList<Item> allItems = DatabaseManager::instance().getAllItems();
    for (const Item &item : allItems) {
        const QString normalizedType = normalizeForSearch(item.type);
        const QString normalizedName = normalizeForSearch(item.name);
        const bool isShield = normalizedName.contains(QStringLiteral("щит")) || normalizedType.contains(QStringLiteral("щит"));

        int baseAc = 10;
        int dexCap = -1;
        bool usesDex = false;
        int shieldBonus = 0;
        if (!parseArmorAcInfo(item, isShield, &baseAc, &dexCap, &usesDex, &shieldBonus)) {
            continue;
        }
        const QString itemName = item.name.trimmed();
        if (itemName.isEmpty()) {
            continue;
        }

        if (isShield) {
            if (shieldBonus <= 0 || usedShieldNames.contains(itemName.toLower())) {
                continue;
            }
            usedShieldNames.insert(itemName.toLower());
            shieldSourceCombo->addItem(QStringLiteral("%1 (+%2)").arg(itemName).arg(shieldBonus), shieldBonus);
            continue;
        }

        if (!normalizedType.contains(QStringLiteral("доспех")) && !normalizedName.contains(QStringLiteral("доспех"))) {
            continue;
        }
        if (usedArmorNames.contains(itemName.toLower())) {
            continue;
        }
        usedArmorNames.insert(itemName.toLower());
        const QString display = armorDisplayText(itemName, baseAc, dexCap, usesDex);
        armorSourceCombo->addItem(display);
        const int index = armorSourceCombo->count() - 1;
        armorSourceCombo->setItemData(index, baseAc, Qt::UserRole);
        armorSourceCombo->setItemData(index, usesDex ? dexCap : 0, Qt::UserRole + 1);
    }

    if (armorSourceCombo->count() == 1) {
        auto addArmorSource = [this](const QString &label, int base, int cap) {
            armorSourceCombo->addItem(label);
            const int idx = armorSourceCombo->count() - 1;
            armorSourceCombo->setItemData(idx, base, Qt::UserRole);
            armorSourceCombo->setItemData(idx, cap, Qt::UserRole + 1);
        };
        addArmorSource(QStringLiteral("Кожаный доспех (11 + ЛОВ)"), 11, -1);
        addArmorSource(QStringLiteral("Чешуйчатый доспех (14 + ЛОВ, макс. +2)"), 14, 2);
        addArmorSource(QStringLiteral("Латы (18)"), 18, 0);
    }
    if (shieldSourceCombo->count() == 1) {
        shieldSourceCombo->addItem(QStringLiteral("Щит (+2)"), 2);
    }

    int restoreArmorIndex = armorSourceCombo->findText(previousArmor, Qt::MatchExactly);
    if (restoreArmorIndex < 0) {
        const QString prefix = previousArmor.section(QLatin1Char('('), 0, 0).trimmed();
        if (!prefix.isEmpty()) {
            for (int i = 0; i < armorSourceCombo->count(); ++i) {
                if (armorSourceCombo->itemText(i).startsWith(prefix, Qt::CaseInsensitive)) {
                    restoreArmorIndex = i;
                    break;
                }
            }
        }
    }
    armorSourceCombo->setCurrentIndex(restoreArmorIndex >= 0 ? restoreArmorIndex : 0);

    int restoreShieldIndex = shieldSourceCombo->findText(previousShield, Qt::MatchExactly);
    if (restoreShieldIndex < 0) {
        const QString prefix = previousShield.section(QLatin1Char('('), 0, 0).trimmed();
        if (!prefix.isEmpty()) {
            for (int i = 0; i < shieldSourceCombo->count(); ++i) {
                if (shieldSourceCombo->itemText(i).startsWith(prefix, Qt::CaseInsensitive)) {
                    restoreShieldIndex = i;
                    break;
                }
            }
        }
    }
    shieldSourceCombo->setCurrentIndex(restoreShieldIndex >= 0 ? restoreShieldIndex : 0);
}

void NpcCreatorWidget::applyDerivedFieldsFromWorkCharacter()
{
    if (!workCharacter) {
        return;
    }

    const QStringList saveEntries = splitCsv(savesEdit ? savesEdit->toPlainText() : QString());
    const QStringList skillEntries = splitCsv(skillsEdit ? skillsEdit->toPlainText() : QString());
    const bool proficientInPerception = skillEntries.contains(QStringLiteral("Восприятие"), Qt::CaseInsensitive)
                                        || workCharacter->skillProficiencies.contains(QStringLiteral("Восприятие"), Qt::CaseInsensitive);
    const int perceptionBase = Character::abilityModifier(workCharacter->wisdom);
    const int passivePerception = 10 + perceptionBase + (proficientInPerception ? workCharacter->proficiencyBonus : 0);

    QStringList savesTotals;
    for (const QString &save : saveEntries) {
        const QString key = abilityKeyForText(save);
        if (key.isEmpty()) {
            continue;
        }
        const int total = Character::abilityModifier(abilityScoreByKey(workCharacter, key)) + workCharacter->proficiencyBonus;
        savesTotals << QStringLiteral("%1 %2").arg(abilityShortLabel(key), signedValue(total));
    }

    const QList<QPair<QString, QString>> skillToAbility = {
        {QStringLiteral("Акробатика"), QStringLiteral("dex")},
        {QStringLiteral("Анализ"), QStringLiteral("int")},
        {QStringLiteral("Атлетика"), QStringLiteral("str")},
        {QStringLiteral("Восприятие"), QStringLiteral("wis")},
        {QStringLiteral("Выживание"), QStringLiteral("wis")},
        {QStringLiteral("Выступление"), QStringLiteral("cha")},
        {QStringLiteral("Запугивание"), QStringLiteral("cha")},
        {QStringLiteral("История"), QStringLiteral("int")},
        {QStringLiteral("Ловкость рук"), QStringLiteral("dex")},
        {QStringLiteral("Магия"), QStringLiteral("int")},
        {QStringLiteral("Медицина"), QStringLiteral("wis")},
        {QStringLiteral("Обман"), QStringLiteral("cha")},
        {QStringLiteral("Природа"), QStringLiteral("int")},
        {QStringLiteral("Проницательность"), QStringLiteral("wis")},
        {QStringLiteral("Религия"), QStringLiteral("int")},
        {QStringLiteral("Скрытность"), QStringLiteral("dex")},
        {QStringLiteral("Убеждение"), QStringLiteral("cha")},
        {QStringLiteral("Уход за животными"), QStringLiteral("wis")}
    };

    QStringList skillTotals;
    for (const QString &skill : skillEntries) {
        const QString normalizedSkill = normalizeToken(skill);
        QString abilityKey;
        QString displaySkill = skill;
        for (const auto &pair : skillToAbility) {
            if (normalizeToken(pair.first) == normalizedSkill) {
                abilityKey = pair.second;
                displaySkill = pair.first;
                break;
            }
        }
        if (abilityKey.isEmpty()) {
            continue;
        }
        const int total = Character::abilityModifier(abilityScoreByKey(workCharacter, abilityKey)) + workCharacter->proficiencyBonus;
        skillTotals << QStringLiteral("%1 %2").arg(displaySkill, signedValue(total));
    }

    int autoArmorClass = workCharacter->armorClass;
    if (armorSourceCombo) {
        const int baseAc = armorBaseFromCombo(armorSourceCombo);
        const int dexMod = Character::abilityModifier(workCharacter->dexterity);
        const int dexCap = armorDexCapFromCombo(armorSourceCombo);
        const int dexPart = (dexCap < 0) ? dexMod : qMin(dexMod, dexCap);
        const int shieldBonus = shieldBonusFromCombo(shieldSourceCombo);
        autoArmorClass = qMax(1, baseAc + dexPart + shieldBonus);
    }

    m_lastAutoAc = autoArmorClass;
    m_lastAutoMaxHp = qMax(1, workCharacter->maxHp);
    const int targetAc = qBound(0, m_lastAutoAc + m_acManualAdjustment, 50);
    const int targetMaxHp = qMax(1, m_lastAutoMaxHp + m_hpManualAdjustment);

    QSignalBlocker blockAc(acSpin);
    QSignalBlocker blockMaxHp(maxHpSpin);
    QSignalBlocker blockCurrentHp(currentHpSpin);
    QSignalBlocker blockSpeed(speedSpin);
    QSignalBlocker blockInitiative(initiativeSpin);
    QSignalBlocker blockPb(pbSpin);
    QSignalBlocker blockPassive(passivePerceptionLabel);

    m_internalAutoUpdate = true;
    acSpin->setValue(targetAc);
    maxHpSpin->setValue(targetMaxHp);
    currentHpSpin->setMaximum(qMax(currentHpSpin->maximum(), targetMaxHp));
    currentHpSpin->setValue(qBound(0, currentHpSpin->value(), targetMaxHp));
    m_internalAutoUpdate = false;
    speedSpin->setValue(workCharacter->speed);
    initiativeSpin->setValue(workCharacter->initiative);
    pbSpin->setValue(workCharacter->proficiencyBonus);
    passivePerceptionLabel->setText(QString::number(qMax(1, passivePerception)));
    if (derivedSavesLabel) {
        derivedSavesLabel->setText(savesTotals.isEmpty() ? QStringLiteral("—") : savesTotals.join(QStringLiteral(", ")));
    }
    if (derivedSkillsLabel) {
        derivedSkillsLabel->setText(skillTotals.isEmpty() ? QStringLiteral("—") : skillTotals.join(QStringLiteral(", ")));
    }
    updateAbilityModifiersLabel();
}

void NpcCreatorWidget::recalculateNpcDerivedFields()
{
    if (m_loading || m_recalculating || currentNpcIdValue.isEmpty()) {
        return;
    }

    m_recalculating = true;
    applyFormValuesToWorkCharacter();
    applyDerivedFieldsFromWorkCharacter();
    updateStatblockPreview();
    m_recalculating = false;
}

void NpcCreatorWidget::updateStatblockPreview()
{
    if (!statblockPreview) {
        return;
    }

    NpcEntry entry = readNpcFromForm();
    entry.passivePerception = passivePerceptionLabel ? passivePerceptionLabel->text().toInt() : entry.passivePerception;
    entry.size = sizeCombo ? sizeCombo->currentText().trimmed() : entry.size;
    entry.creatureType = typeEdit ? typeEdit->text().trimmed() : entry.creatureType;
    entry.challengeRating = challengeEdit ? challengeEdit->text().trimmed() : entry.challengeRating;
    entry.experienceReward = xpSpin ? xpSpin->value() : entry.experienceReward;
    entry.armorSource = armorSourceCombo ? armorSourceCombo->currentText().trimmed() : entry.armorSource;
    entry.shieldSource = shieldSourceCombo ? shieldSourceCombo->currentText().trimmed() : entry.shieldSource;
    entry.armorDescription = acTypeEdit ? acTypeEdit->text().trimmed() : entry.armorDescription;
    entry.hitDiceFormula = hitDiceEdit ? hitDiceEdit->text().trimmed() : entry.hitDiceFormula;
    entry.speedDescription = speedDetailsEdit ? speedDetailsEdit->text().trimmed() : entry.speedDescription;
    entry.senses = sensesEdit ? sensesEdit->text().trimmed() : entry.senses;
    entry.damageVulnerabilities = vulnerabilitiesEdit ? vulnerabilitiesEdit->text().trimmed() : entry.damageVulnerabilities;
    entry.damageResistances = resistancesEdit ? resistancesEdit->text().trimmed() : entry.damageResistances;
    entry.damageImmunities = immunitiesEdit ? immunitiesEdit->text().trimmed() : entry.damageImmunities;
    entry.conditionImmunities = conditionImmunitiesEdit ? conditionImmunitiesEdit->text().trimmed() : entry.conditionImmunities;

    QString preview = formatNpcSummary(entry);
    if (derivedSavesLabel && derivedSavesLabel->text() != QStringLiteral("—")) {
        preview += QStringLiteral("\n\nСпасброски (итого): %1").arg(derivedSavesLabel->text());
    }
    if (derivedSkillsLabel && derivedSkillsLabel->text() != QStringLiteral("—")) {
        preview += QStringLiteral("\nНавыки (итого): %1").arg(derivedSkillsLabel->text());
    }
    if (workCharacter && !workCharacter->spellSlotCurrent.isEmpty()) {
        QStringList slotLines;
        QStringList levels = workCharacter->spellSlotCurrent.keys();
        std::sort(levels.begin(), levels.end(), spellLevelKeyLess);
        for (int index = 0; index < levels.size(); ++index) {
            const QString &level = levels.at(index);
            slotLines << QStringLiteral("%1 ур.: %2").arg(level).arg(workCharacter->spellSlotCurrent.value(level));
        }
        if (!slotLines.isEmpty()) {
            preview += QStringLiteral("\nЯчейки заклинаний: %1").arg(slotLines.join(QStringLiteral(", ")));
        }
    }
    statblockPreview->setPlainText(preview);
}

void NpcCreatorWidget::applyWorkCharacterToCurrentNpc()
{
    if (currentNpcIdValue.isEmpty() || !npcs.contains(currentNpcIdValue) || !workCharacter) {
        return;
    }

    NpcEntry entry = npcs.value(currentNpcIdValue);
    entry.listName = listNameEdit->text().trimmed().isEmpty() ? entry.listName : listNameEdit->text().trimmed();
    entry.folderId = npcs.value(currentNpcIdValue).folderId;
    entry.sortOrder = npcs.value(currentNpcIdValue).sortOrder;
    entry.notes = notesEdit->toPlainText().trimmed();
    syncNpcEntryFromCharacter(*workCharacter, entry);
    entry.id = currentNpcIdValue;
    entry.listName = listNameEdit->text().trimmed().isEmpty() ? entry.listName : listNameEdit->text().trimmed();
    entry.notes = notesEdit->toPlainText().trimmed();
    entry.size = sizeCombo->currentText().trimmed();
    entry.creatureType = typeEdit->text().trimmed();
    entry.challengeRating = challengeEdit->text().trimmed();
    entry.experienceReward = xpSpin->value();
    entry.passivePerception = passivePerceptionLabel ? passivePerceptionLabel->text().toInt() : entry.passivePerception;
    entry.armorSource = armorSourceCombo ? armorSourceCombo->currentText().trimmed() : entry.armorSource;
    entry.shieldSource = shieldSourceCombo ? shieldSourceCombo->currentText().trimmed() : entry.shieldSource;
    entry.armorDescription = acTypeEdit->text().trimmed();
    entry.hitDiceFormula = hitDiceEdit->text().trimmed();
    entry.speedDescription = speedDetailsEdit->text().trimmed();
    entry.senses = sensesEdit->text().trimmed();
    entry.damageVulnerabilities = vulnerabilitiesEdit->text().trimmed();
    entry.damageResistances = resistancesEdit->text().trimmed();
    entry.damageImmunities = immunitiesEdit->text().trimmed();
    entry.conditionImmunities = conditionImmunitiesEdit->text().trimmed();
    npcs[currentNpcIdValue] = entry;
    loadNpcIntoForm(entry);
    if (QTreeWidgetItem *item = findTreeItemById(currentNpcIdValue, TreeItemKind::Npc)) {
        QSignalBlocker blocker(npcTree);
        item->setText(0, entry.listName);
    }
}

void NpcCreatorWidget::updateCreationStatusLabel()
{
    if (!creationStatusLabel) {
        return;
    }

    if (currentNpcIdValue.isEmpty()) {
        creationStatusLabel->setText(QStringLiteral("Выберите NPC в списке слева или создайте нового."));
        return;
    }

    QStringList parts;
    parts << QStringLiteral("Уровень: %1").arg(levelSpin ? levelSpin->value() : 1);
    if (!raceEdit->text().trimmed().isEmpty()) {
        parts << QStringLiteral("Раса: %1").arg(raceEdit->text().trimmed());
    }
    if (!classEdit->text().trimmed().isEmpty()) {
        parts << QStringLiteral("Класс: %1").arg(classEdit->text().trimmed());
    }
    if (!challengeEdit->text().trimmed().isEmpty()) {
        parts << QStringLiteral("КУ: %1").arg(challengeEdit->text().trimmed());
    }
    if (workCharacter && !workCharacter->background.trimmed().isEmpty()) {
        parts << QStringLiteral("Предыстория: %1").arg(workCharacter->background.trimmed());
    }
    if (spellsList && spellsList->count() > 0) {
        parts << QStringLiteral("Заклинаний: %1").arg(spellsList->count());
    }

    creationStatusLabel->setText(
        parts.isEmpty()
            ? QStringLiteral("NPC выбран. Нажмите «Создать по правилам D&D», чтобы заполнить лист по правилам.")
            : QStringLiteral("Текущие параметры: %1").arg(parts.join(QStringLiteral(" | "))));
}

void NpcCreatorWidget::runCreationWizard()
{
    if (currentNpcIdValue.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("Создание NPC"),
            QStringLiteral("Сначала выберите или создайте NPC в списке слева."));
        return;
    }

    bool ok = false;
    const int selectedLevel = QInputDialog::getInt(
        this,
        QStringLiteral("Уровень NPC"),
        QStringLiteral("Выберите итоговый уровень NPC по правилам D&D (1-20):"),
        levelSpin ? levelSpin->value() : 1,
        1,
        20,
        1,
        &ok);
    if (!ok) {
        return;
    }

    if (levelSpin) {
        QSignalBlocker blocker(levelSpin);
        levelSpin->setValue(selectedLevel);
    }

    saveCurrentNpcFromForm();
    ensureWorkCharacter();

    workCharacter->setName(nameEdit->text().trimmed().isEmpty() ? listNameEdit->text().trimmed() : nameEdit->text().trimmed());
    workCharacter->alignment = alignmentCombo->currentText().trimmed();
    workCharacter->level = selectedLevel;
    creationService->setTargetLevel(selectedLevel);
    creationService->syncAbilityScoresFromCharacter();

    if (!creationService->runCreationWizard()) {
        return;
    }

    applyWorkCharacterToCurrentNpc();
    recalculateNpcDerivedFields();
    updateCreationStatusLabel();
    persistToDisk();

    QMessageBox::information(
        this,
        QStringLiteral("NPC создан"),
        QStringLiteral("Лист NPC заполнен по правилам D&D. При необходимости отредактируйте внешность, описание и заметки мастера."));
}

void NpcCreatorWidget::setupUi()
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    auto *leftPanel = new QWidget(splitter);
    leftPanel->setMinimumWidth(240);
    leftPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);

    auto *leftButtons = new QHBoxLayout();
    addNpcBtn = new QPushButton(QStringLiteral("+ NPC"), leftPanel);
    addNpcBtn->setProperty("variant", QStringLiteral("accent"));
    addFolderBtn = new QPushButton(QStringLiteral("Папка"), leftPanel);
    addFolderBtn->setToolTip(QStringLiteral("Создать папку"));
    deleteBtn = new QPushButton(QStringLiteral("Удалить"), leftPanel);
    deleteBtn->setProperty("role", QStringLiteral("danger"));
    deleteBtn->setToolTip(QStringLiteral("Удалить выбранное"));
    leftButtons->addWidget(addNpcBtn);
    leftButtons->addWidget(addFolderBtn);
    leftButtons->addStretch();
    leftButtons->addWidget(deleteBtn);
    leftLayout->addLayout(leftButtons);

    npcTree = new NpcTreeWidget(this, leftPanel);
    npcTree->setHeaderHidden(true);
    npcTree->setDragEnabled(true);
    npcTree->setAcceptDrops(true);
    npcTree->setDropIndicatorShown(true);
    npcTree->setDragDropMode(QAbstractItemView::InternalMove);
    npcTree->setDefaultDropAction(Qt::MoveAction);
    npcTree->setSelectionMode(QAbstractItemView::SingleSelection);
    npcTree->setContextMenuPolicy(Qt::CustomContextMenu);
    leftLayout->addWidget(npcTree, 1);

    auto *rightScroll = new QScrollArea(splitter);
    rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);

    formPanel = new QWidget(rightScroll);
    auto *formLayout = new QVBoxLayout(formPanel);

    auto addGroup = [&](const QString &title, QWidget *content) {
        auto *group = new QGroupBox(title, formPanel);
        auto *layout = new QVBoxLayout(group);
        layout->addWidget(content);
        formLayout->addWidget(group);
        return group;
    };

    auto *wizardWidget = new QWidget(formPanel);
    auto *wizardLayout = new QVBoxLayout(wizardWidget);
    creationStatusLabel = new QLabel(
        QStringLiteral("Запустите мастер создания — он проведёт через те же шаги, что и создание персонажа игрока."),
        wizardWidget);
    creationStatusLabel->setWordWrap(true);
    runWizardBtn = new QPushButton(QStringLiteral("Создать по правилам D&D"), wizardWidget);
    wizardLayout->addWidget(creationStatusLabel);
    wizardLayout->addWidget(runWizardBtn);
    addGroup(QStringLiteral("Строгое создание NPC (D&D 5e)"), wizardWidget);

    auto *identityWidget = new QWidget(formPanel);
    auto *identityForm = new QFormLayout(identityWidget);
    listNameEdit = new QLineEdit(identityWidget);
    listNameEdit->setPlaceholderText(QStringLiteral("Имя в списке слева"));
    nameEdit = new QLineEdit(identityWidget);
    raceEdit = new QLineEdit(identityWidget);
    raceEdit->setReadOnly(true);
    raceEdit->setPlaceholderText(QStringLiteral("Выбирается в мастере создания"));
    classEdit = new QLineEdit(identityWidget);
    classEdit->setReadOnly(true);
    classEdit->setPlaceholderText(QStringLiteral("Выбирается в мастере создания"));
    sizeCombo = new QComboBox(identityWidget);
    sizeCombo->addItems({
        QStringLiteral("Крошечный"),
        QStringLiteral("Маленький"),
        QStringLiteral("Средний"),
        QStringLiteral("Большой"),
        QStringLiteral("Огромный"),
        QStringLiteral("Громадный")
    });
    typeEdit = new QLineEdit(identityWidget);
    typeEdit->setPlaceholderText(QStringLiteral("Например: гуманоид, нежить, зверь"));
    alignmentCombo = new QComboBox(identityWidget);
    alignmentCombo->addItems({
        QString(),
        QStringLiteral("Законопослушный добрый"),
        QStringLiteral("Нейтральный добрый"),
        QStringLiteral("Хаотичный добрый"),
        QStringLiteral("Законопослушный нейтральный"),
        QStringLiteral("Истинно нейтральный"),
        QStringLiteral("Хаотичный нейтральный"),
        QStringLiteral("Законопослушный злой"),
        QStringLiteral("Нейтральный злой"),
        QStringLiteral("Хаотичный злой")
    });
    levelSpin = new QSpinBox(identityWidget);
    levelSpin->setRange(1, 20);
    levelSpin->setToolTip(QStringLiteral("Итоговый уровень NPC по правилам D&D (1–20)"));
    challengeEdit = new QLineEdit(identityWidget);
    challengeEdit->setPlaceholderText(QStringLiteral("Например: 1/4, 2, 10"));
    xpSpin = new QSpinBox(identityWidget);
    xpSpin->setRange(0, 2000000);
    identityForm->addRow(QStringLiteral("Имя в списке:"), listNameEdit);
    identityForm->addRow(QStringLiteral("Имя NPC:"), nameEdit);
    identityForm->addRow(QStringLiteral("Размер:"), sizeCombo);
    identityForm->addRow(QStringLiteral("Тип существа:"), typeEdit);
    identityForm->addRow(QStringLiteral("Раса:"), raceEdit);
    identityForm->addRow(QStringLiteral("Класс:"), classEdit);
    identityForm->addRow(QStringLiteral("Мировоззрение:"), alignmentCombo);
    identityForm->addRow(QStringLiteral("Уровень:"), levelSpin);
    identityForm->addRow(QStringLiteral("Опасность (КУ):"), challengeEdit);
    identityForm->addRow(QStringLiteral("Опыт за победу:"), xpSpin);
    addGroup(QStringLiteral("Основное"), identityWidget);

    auto *abilitiesWidget = new QWidget(formPanel);
    auto *abilitiesLayout = new QGridLayout(abilitiesWidget);
    strSpin = makeAbilitySpin(abilitiesWidget);
    dexSpin = makeAbilitySpin(abilitiesWidget);
    conSpin = makeAbilitySpin(abilitiesWidget);
    intSpin = makeAbilitySpin(abilitiesWidget);
    wisSpin = makeAbilitySpin(abilitiesWidget);
    chaSpin = makeAbilitySpin(abilitiesWidget);
    strAbilityLabel = new QLabel(QStringLiteral("СИЛ"), abilitiesWidget);
    dexAbilityLabel = new QLabel(QStringLiteral("ЛОВ"), abilitiesWidget);
    conAbilityLabel = new QLabel(QStringLiteral("ТЕЛ"), abilitiesWidget);
    intAbilityLabel = new QLabel(QStringLiteral("ИНТ"), abilitiesWidget);
    wisAbilityLabel = new QLabel(QStringLiteral("МДР"), abilitiesWidget);
    chaAbilityLabel = new QLabel(QStringLiteral("ХАР"), abilitiesWidget);
    abilitiesLayout->addWidget(strAbilityLabel, 0, 0);
    abilitiesLayout->addWidget(strSpin, 0, 1);
    abilitiesLayout->addWidget(dexAbilityLabel, 0, 2);
    abilitiesLayout->addWidget(dexSpin, 0, 3);
    abilitiesLayout->addWidget(conAbilityLabel, 1, 0);
    abilitiesLayout->addWidget(conSpin, 1, 1);
    abilitiesLayout->addWidget(intAbilityLabel, 1, 2);
    abilitiesLayout->addWidget(intSpin, 1, 3);
    abilitiesLayout->addWidget(wisAbilityLabel, 2, 0);
    abilitiesLayout->addWidget(wisSpin, 2, 1);
    abilitiesLayout->addWidget(chaAbilityLabel, 2, 2);
    abilitiesLayout->addWidget(chaSpin, 2, 3);
    addGroup(QStringLiteral("Характеристики"), abilitiesWidget);

    auto *combatWidget = new QWidget(formPanel);
    auto *combatForm = new QFormLayout(combatWidget);
    acSpin = new QSpinBox(combatWidget);
    acSpin->setRange(0, 50);
    acSpin->setValue(10);
    armorSourceCombo = new QComboBox(combatWidget);
    shieldSourceCombo = new QComboBox(combatWidget);
    reloadArmorSourcesFromItems();
    acTypeEdit = new QLineEdit(combatWidget);
    acTypeEdit->setPlaceholderText(QStringLiteral("Примечание к КД (магия, особенности, бонусы)"));
    maxHpSpin = new QSpinBox(combatWidget);
    maxHpSpin->setRange(1, 9999);
    hitDiceEdit = new QLineEdit(combatWidget);
    hitDiceEdit->setPlaceholderText(QStringLiteral("Например: 8к8 + 16"));
    currentHpSpin = new QSpinBox(combatWidget);
    currentHpSpin->setRange(0, 9999);
    speedSpin = new QSpinBox(combatWidget);
    speedSpin->setRange(0, 500);
    speedSpin->setValue(30);
    speedSpin->setReadOnly(true);
    speedSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    speedDetailsEdit = new QLineEdit(combatWidget);
    speedDetailsEdit->setPlaceholderText(QStringLiteral("Например: 30 фт., полет 60 фт."));
    initiativeSpin = new QSpinBox(combatWidget);
    initiativeSpin->setRange(-20, 20);
    initiativeSpin->setReadOnly(true);
    initiativeSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    pbSpin = new QSpinBox(combatWidget);
    pbSpin->setRange(0, 10);
    pbSpin->setValue(2);
    pbSpin->setReadOnly(true);
    pbSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    derivedSavesLabel = new QLabel(QStringLiteral("—"), combatWidget);
    derivedSavesLabel->setWordWrap(true);
    derivedSkillsLabel = new QLabel(QStringLiteral("—"), combatWidget);
    derivedSkillsLabel->setWordWrap(true);
    passivePerceptionLabel = new QLabel(QStringLiteral("10"), combatWidget);
    sensesEdit = new QLineEdit(combatWidget);
    sensesEdit->setPlaceholderText(QStringLiteral("Например: тёмное зрение 60 фт."));
    vulnerabilitiesEdit = new QLineEdit(combatWidget);
    resistancesEdit = new QLineEdit(combatWidget);
    immunitiesEdit = new QLineEdit(combatWidget);
    conditionImmunitiesEdit = new QLineEdit(combatWidget);
    vulnerabilitiesEdit->setPlaceholderText(QStringLiteral("Огонь, излучение и т.д."));
    resistancesEdit->setPlaceholderText(QStringLiteral("Холод, дробящий от немагического и т.д."));
    immunitiesEdit->setPlaceholderText(QStringLiteral("Яд, психический и т.д."));
    conditionImmunitiesEdit->setPlaceholderText(QStringLiteral("Очарованный, испуганный и т.д."));
    combatForm->addRow(QStringLiteral("КД:"), acSpin);
    combatForm->addRow(QStringLiteral("Источник КД (броня):"), armorSourceCombo);
    combatForm->addRow(QStringLiteral("Источник КД (щит):"), shieldSourceCombo);
    combatForm->addRow(QStringLiteral("Примечание к КД:"), acTypeEdit);
    combatForm->addRow(QStringLiteral("Хиты (макс.):"), maxHpSpin);
    combatForm->addRow(QStringLiteral("Формула хитов:"), hitDiceEdit);
    combatForm->addRow(QStringLiteral("Хиты (тек.):"), currentHpSpin);
    combatForm->addRow(QStringLiteral("Скорость:"), speedSpin);
    combatForm->addRow(QStringLiteral("Детали скорости:"), speedDetailsEdit);
    combatForm->addRow(QStringLiteral("Инициатива:"), initiativeSpin);
    combatForm->addRow(QStringLiteral("БМ:"), pbSpin);
    combatForm->addRow(QStringLiteral("Спасброски (итого):"), derivedSavesLabel);
    combatForm->addRow(QStringLiteral("Навыки (итого):"), derivedSkillsLabel);
    combatForm->addRow(QStringLiteral("Пассивное восприятие:"), passivePerceptionLabel);
    combatForm->addRow(QStringLiteral("Чувства:"), sensesEdit);
    combatForm->addRow(QStringLiteral("Уязвимости урона:"), vulnerabilitiesEdit);
    combatForm->addRow(QStringLiteral("Сопротивления урону:"), resistancesEdit);
    combatForm->addRow(QStringLiteral("Иммунитеты к урону:"), immunitiesEdit);
    combatForm->addRow(QStringLiteral("Иммунитеты к состояниям:"), conditionImmunitiesEdit);
    addGroup(QStringLiteral("Бой"), combatWidget);

    auto *appearanceWidget = new QWidget(formPanel);
    auto *appearanceForm = new QFormLayout(appearanceWidget);
    ageEdit = new QLineEdit(appearanceWidget);
    heightEdit = new QLineEdit(appearanceWidget);
    weightEdit = new QLineEdit(appearanceWidget);
    skinEdit = new QLineEdit(appearanceWidget);
    hairEdit = new QLineEdit(appearanceWidget);
    appearanceEdit = new QTextEdit(appearanceWidget);
    appearanceEdit->setMaximumHeight(100);
    appearanceForm->addRow(QStringLiteral("Возраст:"), ageEdit);
    appearanceForm->addRow(QStringLiteral("Рост:"), heightEdit);
    appearanceForm->addRow(QStringLiteral("Вес:"), weightEdit);
    appearanceForm->addRow(QStringLiteral("Кожа:"), skinEdit);
    appearanceForm->addRow(QStringLiteral("Волосы:"), hairEdit);
    appearanceForm->addRow(QStringLiteral("Внешность:"), appearanceEdit);
    addGroup(QStringLiteral("Внешность"), appearanceWidget);

    descriptionEdit = new QTextEdit(formPanel);
    descriptionEdit->setMaximumHeight(120);
    addGroup(QStringLiteral("Описание"), descriptionEdit);

    notesEdit = new QTextEdit(formPanel);
    notesEdit->setMaximumHeight(120);
    addGroup(QStringLiteral("Заметки мастера"), notesEdit);

    auto *profWidget = new QWidget(formPanel);
    auto *profLayout = new QVBoxLayout(profWidget);
    languagesEdit = new QPlainTextEdit(profWidget);
    languagesEdit->setPlaceholderText(QStringLiteral("По одному языку на строку"));
    languagesEdit->setMaximumHeight(70);
    skillsEdit = new QPlainTextEdit(profWidget);
    skillsEdit->setPlaceholderText(QStringLiteral("Владение навыками"));
    skillsEdit->setMaximumHeight(70);
    savesEdit = new QPlainTextEdit(profWidget);
    savesEdit->setPlaceholderText(QStringLiteral("Спасброски"));
    savesEdit->setMaximumHeight(70);
    armorEdit = new QPlainTextEdit(profWidget);
    armorEdit->setMaximumHeight(70);
    weaponsEdit = new QPlainTextEdit(profWidget);
    weaponsEdit->setMaximumHeight(70);
    profLayout->addWidget(new QLabel(QStringLiteral("Языки"), profWidget));
    profLayout->addWidget(languagesEdit);
    profLayout->addWidget(new QLabel(QStringLiteral("Навыки"), profWidget));
    profLayout->addWidget(skillsEdit);
    profLayout->addWidget(new QLabel(QStringLiteral("Спасброски"), profWidget));
    profLayout->addWidget(savesEdit);
    profLayout->addWidget(new QLabel(QStringLiteral("Доспехи"), profWidget));
    profLayout->addWidget(armorEdit);
    profLayout->addWidget(new QLabel(QStringLiteral("Оружие"), profWidget));
    profLayout->addWidget(weaponsEdit);
    addGroup(QStringLiteral("Владения"), profWidget);

    attacksList = new QListWidget(formPanel);
    attacksList->setMaximumHeight(120);
    auto *attacksRow = new QHBoxLayout();
    auto *attacksPanel = new QWidget(formPanel);
    auto *attacksLayout = new QVBoxLayout(attacksPanel);
    attacksLayout->addWidget(attacksList);
    auto *addAttackBtn = new QPushButton(QStringLiteral("Добавить атаку"), attacksPanel);
    auto *removeAttackBtn = new QPushButton(QStringLiteral("Удалить"), attacksPanel);
    attacksRow->addWidget(addAttackBtn);
    attacksRow->addWidget(removeAttackBtn);
    attacksLayout->addLayout(attacksRow);
    addGroup(QStringLiteral("Атаки"), attacksPanel);

    traitsList = new QListWidget(formPanel);
    traitsList->setMaximumHeight(120);
    auto *traitsPanel = new QWidget(formPanel);
    auto *traitsLayout = new QVBoxLayout(traitsPanel);
    traitsLayout->addWidget(traitsList);
    auto *traitsRow = new QHBoxLayout();
    auto *addTraitBtn = new QPushButton(QStringLiteral("Добавить умение"), traitsPanel);
    auto *removeTraitBtn = new QPushButton(QStringLiteral("Удалить"), traitsPanel);
    traitsRow->addWidget(addTraitBtn);
    traitsRow->addWidget(removeTraitBtn);
    traitsLayout->addLayout(traitsRow);
    addGroup(QStringLiteral("Особенности"), traitsPanel);

    spellsList = new QListWidget(formPanel);
    spellsList->setMaximumHeight(140);
    auto *spellsPanel = new QWidget(formPanel);
    auto *spellsLayout = new QVBoxLayout(spellsPanel);
    spellsLayout->addWidget(spellsList);
    auto *spellsRow = new QHBoxLayout();
    auto *addSpellBtn = new QPushButton(QStringLiteral("Добавить заклинание"), spellsPanel);
    auto *removeSpellBtn = new QPushButton(QStringLiteral("Удалить"), spellsPanel);
    spellsRow->addWidget(addSpellBtn);
    spellsRow->addWidget(removeSpellBtn);
    spellsLayout->addLayout(spellsRow);
    addGroup(QStringLiteral("Заклинания"), spellsPanel);

    inventoryList = new QListWidget(formPanel);
    inventoryList->setMaximumHeight(140);
    auto *inventoryPanel = new QWidget(formPanel);
    auto *inventoryLayout = new QVBoxLayout(inventoryPanel);
    inventoryLayout->addWidget(inventoryList);
    auto *inventoryRow = new QHBoxLayout();
    auto *addItemBtn = new QPushButton(QStringLiteral("Добавить предмет"), inventoryPanel);
    auto *removeItemBtn = new QPushButton(QStringLiteral("Удалить"), inventoryPanel);
    inventoryRow->addWidget(addItemBtn);
    inventoryRow->addWidget(removeItemBtn);
    inventoryLayout->addLayout(inventoryRow);
    addGroup(QStringLiteral("Инвентарь"), inventoryPanel);

    statblockPreview = new QTextEdit(formPanel);
    statblockPreview->setReadOnly(true);
    statblockPreview->setMinimumHeight(260);
    statblockPreview->setPlaceholderText(QStringLiteral("Здесь появится готовый статблок NPC."));
    addGroup(QStringLiteral("Предпросмотр статблока NPC"), statblockPreview);

    formLayout->addStretch();
    rightScroll->setWidget(formPanel);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightScroll);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({320, 960});

    mainLayout->addWidget(splitter);

    connect(addNpcBtn, &QPushButton::clicked, this, &NpcCreatorWidget::createNpc);
    connect(addFolderBtn, &QPushButton::clicked, this, &NpcCreatorWidget::createFolder);
    connect(deleteBtn, &QPushButton::clicked, this, &NpcCreatorWidget::deleteSelectedTreeItem);
    connect(npcTree, &QTreeWidget::itemSelectionChanged, this, &NpcCreatorWidget::onTreeSelectionChanged);
    connect(npcTree, &QTreeWidget::itemExpanded, this, &NpcCreatorWidget::onTreeItemExpanded);
    connect(npcTree, &QTreeWidget::itemCollapsed, this, &NpcCreatorWidget::onTreeItemCollapsed);
    connect(npcTree->model(), &QAbstractItemModel::rowsMoved, this, &NpcCreatorWidget::onTreeStructureChanged);
    connect(npcTree, &QTreeWidget::customContextMenuRequested, this, &NpcCreatorWidget::showTreeContextMenu);
    connect(npcTree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        if (item && item->flags().testFlag(Qt::ItemIsSelectable)) {
            npcTree->setCurrentItem(item);
            renameSelectedTreeItem();
        }
    });

    connect(addAttackBtn, &QPushButton::clicked, this, &NpcCreatorWidget::addAttack);
    connect(removeAttackBtn, &QPushButton::clicked, this, &NpcCreatorWidget::removeAttack);
    connect(addTraitBtn, &QPushButton::clicked, this, &NpcCreatorWidget::addTrait);
    connect(removeTraitBtn, &QPushButton::clicked, this, &NpcCreatorWidget::removeTrait);
    connect(addSpellBtn, &QPushButton::clicked, this, &NpcCreatorWidget::addSpell);
    connect(removeSpellBtn, &QPushButton::clicked, this, &NpcCreatorWidget::removeSpell);
    connect(addItemBtn, &QPushButton::clicked, this, &NpcCreatorWidget::addInventoryItem);
    connect(removeItemBtn, &QPushButton::clicked, this, &NpcCreatorWidget::removeInventoryItem);
    connect(runWizardBtn, &QPushButton::clicked, this, &NpcCreatorWidget::runCreationWizard);

    const auto hookFormChange = [this]() { onFormFieldChanged(); };
    connect(listNameEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(nameEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(sizeCombo, &QComboBox::currentTextChanged, this, hookFormChange);
    connect(typeEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(alignmentCombo, &QComboBox::currentTextChanged, this, hookFormChange);
    connect(levelSpin, qOverload<int>(&QSpinBox::valueChanged), this, hookFormChange);
    connect(challengeEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(xpSpin, qOverload<int>(&QSpinBox::valueChanged), this, hookFormChange);
    for (QSpinBox *spin : {strSpin, dexSpin, conSpin, intSpin, wisSpin, chaSpin, currentHpSpin, acSpin, maxHpSpin}) {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, hookFormChange);
    }
    connect(armorSourceCombo, &QComboBox::currentTextChanged, this, hookFormChange);
    connect(shieldSourceCombo, &QComboBox::currentTextChanged, this, hookFormChange);
    connect(acTypeEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(hitDiceEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(speedDetailsEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(sensesEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(vulnerabilitiesEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(resistancesEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(immunitiesEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(conditionImmunitiesEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(ageEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(heightEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(weightEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(skinEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(hairEdit, &QLineEdit::textChanged, this, hookFormChange);
    connect(appearanceEdit, &QTextEdit::textChanged, this, hookFormChange);
    connect(descriptionEdit, &QTextEdit::textChanged, this, hookFormChange);
    connect(notesEdit, &QTextEdit::textChanged, this, hookFormChange);
    connect(languagesEdit, &QPlainTextEdit::textChanged, this, hookFormChange);
    connect(skillsEdit, &QPlainTextEdit::textChanged, this, hookFormChange);
    connect(savesEdit, &QPlainTextEdit::textChanged, this, hookFormChange);
    connect(armorEdit, &QPlainTextEdit::textChanged, this, hookFormChange);
    connect(weaponsEdit, &QPlainTextEdit::textChanged, this, hookFormChange);

    for (QSpinBox *spin : {strSpin, dexSpin, conSpin, intSpin, wisSpin, chaSpin}) {
        connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, [this]() {
            m_acManualAdjustment = 0;
            m_hpManualAdjustment = 0;
            updateAbilityModifiersLabel();
        });
    }
    updateAbilityModifiersLabel();

    connect(attacksList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item || !attacksList) {
            return;
        }
        const QPoint anchor = attacksList->viewport()->mapToGlobal(attacksList->visualItemRect(item).bottomLeft());
        showDetailPopup(this, item->text(), item->data(Qt::UserRole + 10).toString(), anchor);
    });
    connect(traitsList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item || !traitsList) {
            return;
        }
        const QString title = item->data(Qt::UserRole).toString().trimmed();
        const QString body = item->data(Qt::UserRole + 1).toString();
        const QPoint anchor = traitsList->viewport()->mapToGlobal(traitsList->visualItemRect(item).bottomLeft());
        showDetailPopup(this, title.isEmpty() ? item->text() : title, body, anchor);
    });
    connect(spellsList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item || !spellsList) {
            return;
        }
        const QString spellName = item->data(Qt::UserRole).toString().trimmed();
        if (spellName.isEmpty()) {
            return;
        }

        QString description;
        for (const Spell &spell : DatabaseManager::instance().getAllSpells()) {
            if (spell.name.compare(spellName, Qt::CaseInsensitive) == 0) {
                description = spell.description.trimmed();
                if (!spell.school.trimmed().isEmpty()) {
                    description = QStringLiteral("Школа: %1\n\n%2").arg(spell.school.trimmed(), description);
                }
                break;
            }
        }

        const QPoint anchor = spellsList->viewport()->mapToGlobal(spellsList->visualItemRect(item).bottomLeft());
        showDetailPopup(this, spellName, description, anchor);
    });
    connect(inventoryList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item || !inventoryList) {
            return;
        }
        const QString itemName = item->data(Qt::UserRole).toString().trimmed();
        if (itemName.isEmpty()) {
            return;
        }

        QString description = item->data(Qt::UserRole + 2).toString().trimmed();
        if (description.isEmpty()) {
            for (const Item &dbItem : DatabaseManager::instance().getAllItems()) {
                if (dbItem.name.compare(itemName, Qt::CaseInsensitive) == 0) {
                    description = dbItem.description.trimmed();
                    break;
                }
            }
        }

        const QPoint anchor = inventoryList->viewport()->mapToGlobal(inventoryList->visualItemRect(item).bottomLeft());
        showDetailPopup(this, itemName, description, anchor);
    });

    formPanel->setEnabled(false);
    updateToolbarState();
}

void NpcCreatorWidget::ensureStorageReady()
{
    if (!m_storageScope.isEmpty()) {
        return;
    }
    setStorageScope(QStringLiteral("master_default"));
}

void NpcCreatorWidget::setFormEnabled(bool enabled)
{
    if (formPanel) {
        formPanel->setEnabled(enabled);
    }
}

void NpcCreatorWidget::updateToolbarState()
{
    const QTreeWidgetItem *item = npcTree ? npcTree->currentItem() : nullptr;
    if (deleteBtn) {
        deleteBtn->setEnabled(item != nullptr);
    }
}

QString NpcCreatorWidget::targetFolderIdForNewNpc() const
{
    QTreeWidgetItem *item = npcTree ? npcTree->currentItem() : nullptr;
    if (!item) {
        return {};
    }

    const TreeItemKind kind = static_cast<TreeItemKind>(item->data(0, RoleKind).toInt());
    if (kind == TreeItemKind::Folder) {
        return item->data(0, RoleId).toString();
    }

    if (kind == TreeItemKind::Npc && item->parent()) {
        const TreeItemKind parentKind = static_cast<TreeItemKind>(item->parent()->data(0, RoleKind).toInt());
        if (parentKind == TreeItemKind::Folder) {
            return item->parent()->data(0, RoleId).toString();
        }
    }

    return {};
}

int NpcCreatorWidget::nextSortOrderInFolder(const QString &folderId) const
{
    int maxOrder = -1;
    for (const NpcEntry &entry : npcs) {
        if (entry.folderId == folderId && entry.sortOrder > maxOrder) {
            maxOrder = entry.sortOrder;
        }
    }
    return maxOrder + 1;
}

bool NpcCreatorWidget::listNameExists(const QString &name, const QString &exceptId) const
{
    const QString normalized = name.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    for (auto it = npcs.constBegin(); it != npcs.constEnd(); ++it) {
        if (it.key() == exceptId) {
            continue;
        }
        if (it.value().listName.compare(normalized, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

bool NpcCreatorWidget::folderNameExists(const QString &name, const QString &exceptId) const
{
    const QString normalized = name.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }
    for (auto it = folders.constBegin(); it != folders.constEnd(); ++it) {
        if (it.key() == exceptId) {
            continue;
        }
        if (it.value().name.compare(normalized, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString NpcCreatorWidget::currentNpcId() const
{
    return currentNpcIdValue;
}

QTreeWidgetItem *NpcCreatorWidget::findTreeItemById(const QString &id, TreeItemKind kind) const
{
    const auto walk = [&](QTreeWidgetItem *parent, auto &&self) -> QTreeWidgetItem * {
        const int count = parent ? parent->childCount() : npcTree->topLevelItemCount();
        for (int index = 0; index < count; ++index) {
            QTreeWidgetItem *item = parent ? parent->child(index) : npcTree->topLevelItem(index);
            const auto itemKind = static_cast<TreeItemKind>(item->data(0, RoleKind).toInt());
            if (item->data(0, RoleId).toString() == id && itemKind == kind) {
                return item;
            }
            if (QTreeWidgetItem *found = self(item, self)) {
                return found;
            }
        }
        return nullptr;
    };
    return walk(nullptr, walk);
}

void NpcCreatorWidget::rebuildTree()
{
    m_loading = true;
    QSignalBlocker blocker(npcTree);
    npcTree->clear();

    QList<NpcFolder> folderList = folders.values();
    std::sort(folderList.begin(), folderList.end(), [](const NpcFolder &a, const NpcFolder &b) {
        if (a.sortOrder != b.sortOrder) {
            return a.sortOrder < b.sortOrder;
        }
        return a.name.localeAwareCompare(b.name) < 0;
    });

    auto appendNpcItem = [&](QTreeWidgetItem *parent, const NpcEntry &entry) {
        QTreeWidgetItem *item = parent
            ? new QTreeWidgetItem(parent, {entry.listName})
            : new QTreeWidgetItem(npcTree, {entry.listName});
        item->setData(0, RoleKind, static_cast<int>(TreeItemKind::Npc));
        item->setData(0, RoleId, entry.id);
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    };

    for (const NpcFolder &folder : folderList) {
        auto *folderItem = new QTreeWidgetItem(npcTree, {folder.name});
        folderItem->setData(0, RoleKind, static_cast<int>(TreeItemKind::Folder));
        folderItem->setData(0, RoleId, folder.id);
        folderItem->setFlags(folderItem->flags() | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        folderItem->setExpanded(folder.expanded);

        QList<NpcEntry> children;
        for (const NpcEntry &entry : npcs) {
            if (entry.folderId == folder.id) {
                children << entry;
            }
        }
        std::sort(children.begin(), children.end(), [](const NpcEntry &a, const NpcEntry &b) {
            if (a.sortOrder != b.sortOrder) {
                return a.sortOrder < b.sortOrder;
            }
            return a.listName.localeAwareCompare(b.listName) < 0;
        });
        for (const NpcEntry &entry : children) {
            appendNpcItem(folderItem, entry);
        }
    }

    QList<NpcEntry> rootNpcs;
    for (const NpcEntry &entry : npcs) {
        if (entry.folderId.trimmed().isEmpty() || !folders.contains(entry.folderId)) {
            rootNpcs << entry;
        }
    }
    std::sort(rootNpcs.begin(), rootNpcs.end(), [](const NpcEntry &a, const NpcEntry &b) {
        if (a.sortOrder != b.sortOrder) {
            return a.sortOrder < b.sortOrder;
        }
        return a.listName.localeAwareCompare(b.listName) < 0;
    });
    for (const NpcEntry &entry : rootNpcs) {
        appendNpcItem(nullptr, entry);
    }

    if (folders.isEmpty() && npcs.isEmpty()) {
        auto *hint = new QTreeWidgetItem(
            npcTree,
            {QStringLiteral("Список пуст. Нажмите + NPC")});
        hint->setFlags(Qt::NoItemFlags);
        hint->setForeground(0, QColor(QStringLiteral("#888888")));
    }

    if (!currentNpcIdValue.isEmpty()) {
        if (QTreeWidgetItem *item = findTreeItemById(currentNpcIdValue, TreeItemKind::Npc)) {
            npcTree->setCurrentItem(item);
        }
    }
    m_loading = false;
    updateToolbarState();
}

void NpcCreatorWidget::syncStructureFromTree()
{
    QMap<QString, NpcFolder> updatedFolders = folders;
    QMap<QString, NpcEntry> updatedNpcs = npcs;

    const auto processLevel = [&](QTreeWidgetItem *parent, const QString &folderId) {
        const int count = parent ? parent->childCount() : npcTree->topLevelItemCount();
        for (int index = 0; index < count; ++index) {
            QTreeWidgetItem *item = parent ? parent->child(index) : npcTree->topLevelItem(index);
            const TreeItemKind kind = static_cast<TreeItemKind>(item->data(0, RoleKind).toInt());
            const QString id = item->data(0, RoleId).toString();

            if (kind == TreeItemKind::Folder) {
                if (updatedFolders.contains(id)) {
                    updatedFolders[id].sortOrder = index;
                    updatedFolders[id].name = item->text(0);
                    updatedFolders[id].expanded = item->isExpanded();
                }
                for (int childIndex = 0; childIndex < item->childCount(); ++childIndex) {
                    QTreeWidgetItem *child = item->child(childIndex);
                    if (static_cast<TreeItemKind>(child->data(0, RoleKind).toInt()) != TreeItemKind::Npc) {
                        continue;
                    }
                    const QString npcId = child->data(0, RoleId).toString();
                    if (!updatedNpcs.contains(npcId)) {
                        continue;
                    }
                    updatedNpcs[npcId].folderId = id;
                    updatedNpcs[npcId].sortOrder = childIndex;
                    updatedNpcs[npcId].listName = child->text(0);
                }
            } else if (kind == TreeItemKind::Npc && updatedNpcs.contains(id)) {
                updatedNpcs[id].folderId = folderId;
                updatedNpcs[id].sortOrder = index;
                updatedNpcs[id].listName = item->text(0);
            }
        }
    };

    processLevel(nullptr, QString());

    folders = updatedFolders;
    npcs = updatedNpcs;
    schedulePersist();
}

void NpcCreatorWidget::selectNpcById(const QString &npcId)
{
    currentNpcIdValue = npcId;
    if (workCharacter) {
        workCharacter->deleteLater();
        workCharacter = nullptr;
    }
    if (QTreeWidgetItem *item = findTreeItemById(npcId, TreeItemKind::Npc)) {
        QSignalBlocker blocker(npcTree);
        npcTree->setCurrentItem(item);
    }
    if (npcs.contains(npcId)) {
        loadNpcIntoForm(npcs.value(npcId));
        setFormEnabled(true);
    } else {
        setFormEnabled(false);
        if (statblockPreview) {
            statblockPreview->clear();
        }
    }
    updateCreationStatusLabel();
    updateToolbarState();
}

void NpcCreatorWidget::createNpc()
{
    ensureStorageReady();

    bool ok = false;
    QString listName = QInputDialog::getText(
        this,
        QStringLiteral("Новый NPC"),
        QStringLiteral("Имя для списка (как NPC будет храниться):"),
        QLineEdit::Normal,
        QStringLiteral("Новый NPC"),
        &ok);
    if (!ok) {
        return;
    }

    listName = listName.trimmed();
    if (listName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Новый NPC"), QStringLiteral("Имя NPC не может быть пустым."));
        return;
    }

    if (listNameExists(listName)) {
        int suffix = 2;
        const QString baseName = listName;
        while (listNameExists(listName)) {
            listName = QStringLiteral("%1 (%2)").arg(baseName).arg(suffix++);
        }
        const auto reply = QMessageBox::question(
            this,
            QStringLiteral("Новый NPC"),
            QStringLiteral("NPC с таким именем уже есть. Создать как «%1»?").arg(listName));
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    saveCurrentNpcFromForm();

    const QString folderId = targetFolderIdForNewNpc();
    NpcEntry entry = NpcEntry::createNew(listName);
    entry.folderId = folderId;
    entry.sortOrder = nextSortOrderInFolder(folderId);
    npcs.insert(entry.id, entry);
    rebuildTree();
    selectNpcById(entry.id);
    persistToDisk();

    const auto reply = QMessageBox::question(
        this,
        QStringLiteral("Создание по правилам D&D"),
        QStringLiteral("Запустить мастер создания для «%1»?\nОн проведёт через характеристики, расу, класс, предысторию и заклинания, как при создании персонажа игрока.")
            .arg(listName));
    if (reply == QMessageBox::Yes) {
        runCreationWizard();
    } else {
        updateCreationStatusLabel();
    }
}

void NpcCreatorWidget::createFolder()
{
    ensureStorageReady();

    bool ok = false;
    QString folderName = QInputDialog::getText(
        this,
        QStringLiteral("Новая папка"),
        QStringLiteral("Название папки:"),
        QLineEdit::Normal,
        QStringLiteral("Папка"),
        &ok);
    if (!ok) {
        return;
    }

    folderName = folderName.trimmed();
    if (folderName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Новая папка"), QStringLiteral("Название папки не может быть пустым."));
        return;
    }

    if (folderNameExists(folderName)) {
        int suffix = 2;
        const QString baseName = folderName;
        while (folderNameExists(folderName)) {
            folderName = QStringLiteral("%1 (%2)").arg(baseName).arg(suffix++);
        }
        const auto reply = QMessageBox::question(
            this,
            QStringLiteral("Новая папка"),
            QStringLiteral("Папка с таким именем уже есть. Создать как «%1»?").arg(folderName));
        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    NpcFolder folder;
    folder.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    folder.name = folderName;
    folder.sortOrder = folders.size();
    folder.expanded = true;
    folders.insert(folder.id, folder);
    rebuildTree();

    if (QTreeWidgetItem *folderItem = findTreeItemById(folder.id, TreeItemKind::Folder)) {
        npcTree->setCurrentItem(folderItem);
        npcTree->expandItem(folderItem);
    }

    setFormEnabled(false);
    updateToolbarState();
    persistToDisk();
}

void NpcCreatorWidget::deleteSelectedTreeItem()
{
    ensureStorageReady();

    QTreeWidgetItem *item = npcTree->currentItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("Удаление"), QStringLiteral("Выберите NPC или папку в списке слева."));
        return;
    }

    const TreeItemKind kind = static_cast<TreeItemKind>(item->data(0, RoleKind).toInt());
    const QString id = item->data(0, RoleId).toString();

    if (kind == TreeItemKind::Folder) {
        const auto reply = QMessageBox::question(
            this,
            QStringLiteral("Удалить папку"),
            QStringLiteral("Удалить папку «%1»?\nNPC из неё переместятся в корень списка.").arg(item->text(0)));
        if (reply != QMessageBox::Yes) {
            return;
        }
        for (auto it = npcs.begin(); it != npcs.end(); ++it) {
            if (it.value().folderId == id) {
                it.value().folderId.clear();
            }
        }
        folders.remove(id);
        if (!npcs.contains(currentNpcIdValue)) {
            currentNpcIdValue.clear();
            setFormEnabled(false);
        }
    } else {
        const auto reply = QMessageBox::question(
            this,
            QStringLiteral("Удалить NPC"),
            QStringLiteral("Удалить NPC «%1»?\nЭто действие нельзя отменить.").arg(item->text(0)));
        if (reply != QMessageBox::Yes) {
            return;
        }
        npcs.remove(id);
        currentNpcIdValue.clear();
        setFormEnabled(false);
    }

    rebuildTree();
    persistToDisk();
}

void NpcCreatorWidget::renameSelectedTreeItem()
{
    QTreeWidgetItem *item = npcTree->currentItem();
    if (!item) {
        return;
    }

    const TreeItemKind kind = static_cast<TreeItemKind>(item->data(0, RoleKind).toInt());
    const QString id = item->data(0, RoleId).toString();
    bool ok = false;
    const QString newName = QInputDialog::getText(
        this,
        QStringLiteral("Переименовать"),
        kind == TreeItemKind::Folder ? QStringLiteral("Новое имя папки:") : QStringLiteral("Новое имя в списке:"),
        QLineEdit::Normal,
        item->text(0),
        &ok);
    if (!ok || newName.trimmed().isEmpty()) {
        return;
    }

    item->setText(0, newName.trimmed());
    if (kind == TreeItemKind::Folder && folders.contains(id)) {
        folders[id].name = newName.trimmed();
    } else if (kind == TreeItemKind::Npc && npcs.contains(id)) {
        npcs[id].listName = newName.trimmed();
        if (id == currentNpcIdValue) {
            listNameEdit->setText(newName.trimmed());
        }
    }
    schedulePersist();
}

void NpcCreatorWidget::showTreeContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = npcTree->itemAt(pos);
    if (!item) {
        return;
    }
    npcTree->setCurrentItem(item);

    QMenu menu(this);
    menu.addAction(QStringLiteral("Переименовать"), this, &NpcCreatorWidget::renameSelectedTreeItem);
    menu.addAction(QStringLiteral("Удалить"), this, &NpcCreatorWidget::deleteSelectedTreeItem);
    menu.exec(npcTree->viewport()->mapToGlobal(pos));
}

void NpcCreatorWidget::onTreeSelectionChanged()
{
    if (m_loading) {
        return;
    }

    saveCurrentNpcFromForm();

    QTreeWidgetItem *item = npcTree->currentItem();
    if (!item) {
        currentNpcIdValue.clear();
        setFormEnabled(false);
        if (statblockPreview) {
            statblockPreview->clear();
        }
        updateToolbarState();
        return;
    }

    if (static_cast<TreeItemKind>(item->data(0, RoleKind).toInt()) != TreeItemKind::Npc) {
        currentNpcIdValue.clear();
        setFormEnabled(false);
        if (statblockPreview) {
            statblockPreview->clear();
        }
        updateToolbarState();
        return;
    }

    selectNpcById(item->data(0, RoleId).toString());
}

void NpcCreatorWidget::onTreeStructureChanged()
{
    if (m_loading) {
        return;
    }
    syncStructureFromTree();
}

void NpcCreatorWidget::onTreeItemExpanded(QTreeWidgetItem *item)
{
    if (!item || static_cast<TreeItemKind>(item->data(0, RoleKind).toInt()) != TreeItemKind::Folder) {
        return;
    }
    const QString id = item->data(0, RoleId).toString();
    if (folders.contains(id)) {
        folders[id].expanded = true;
        schedulePersist();
    }
}

void NpcCreatorWidget::onTreeItemCollapsed(QTreeWidgetItem *item)
{
    if (!item || static_cast<TreeItemKind>(item->data(0, RoleKind).toInt()) != TreeItemKind::Folder) {
        return;
    }
    const QString id = item->data(0, RoleId).toString();
    if (folders.contains(id)) {
        folders[id].expanded = false;
        schedulePersist();
    }
}

void NpcCreatorWidget::onFormFieldChanged()
{
    if (m_loading || currentNpcIdValue.isEmpty()) {
        return;
    }
    if (!m_internalAutoUpdate) {
        if (sender() == acSpin) {
            m_acManualAdjustment = acSpin->value() - m_lastAutoAc;
        } else if (sender() == maxHpSpin) {
            m_hpManualAdjustment = maxHpSpin->value() - m_lastAutoMaxHp;
        } else if (sender() == armorSourceCombo || sender() == shieldSourceCombo) {
            m_acManualAdjustment = 0;
        } else if (sender() == levelSpin) {
            m_hpManualAdjustment = 0;
        }
    }
    recalculateNpcDerivedFields();
    saveCurrentNpcFromForm();
    schedulePersist();
}

void NpcCreatorWidget::saveCurrentNpcFromForm()
{
    if (currentNpcIdValue.isEmpty() || !npcs.contains(currentNpcIdValue)) {
        return;
    }

    NpcEntry entry = readNpcFromForm();
    entry.id = currentNpcIdValue;
    entry.listName = listNameEdit->text().trimmed().isEmpty() ? entry.name : listNameEdit->text().trimmed();
    entry.folderId = npcs.value(currentNpcIdValue).folderId;
    entry.sortOrder = npcs.value(currentNpcIdValue).sortOrder;
    if (workCharacter && workCharacter->property("npcId").toString() == currentNpcIdValue) {
        entry.characterRulesJson = workCharacter->toJson();
    } else {
        entry.characterRulesJson = npcs.value(currentNpcIdValue).characterRulesJson;
    }
    npcs[currentNpcIdValue] = entry;

    if (QTreeWidgetItem *item = findTreeItemById(currentNpcIdValue, TreeItemKind::Npc)) {
        QSignalBlocker blocker(npcTree);
        item->setText(0, entry.listName);
    }
}

NpcEntry NpcCreatorWidget::readNpcFromForm() const
{
    NpcEntry entry;
    entry.listName = listNameEdit->text().trimmed();
    entry.name = nameEdit->text().trimmed();
    entry.race = raceEdit->text().trimmed();
    entry.characterClass = classEdit->text().trimmed();
    entry.size = sizeCombo->currentText().trimmed();
    entry.creatureType = typeEdit->text().trimmed();
    entry.challengeRating = challengeEdit->text().trimmed();
    entry.experienceReward = xpSpin->value();
    entry.alignment = alignmentCombo->currentText().trimmed();
    entry.level = levelSpin->value();

    entry.strength = strSpin->value();
    entry.dexterity = dexSpin->value();
    entry.constitution = conSpin->value();
    entry.intelligence = intSpin->value();
    entry.wisdom = wisSpin->value();
    entry.charisma = chaSpin->value();

    entry.armorClass = acSpin->value();
    entry.maxHp = maxHpSpin->value();
    entry.currentHp = currentHpSpin->value();
    entry.speed = speedSpin->value();
    entry.initiative = initiativeSpin->value();
    entry.proficiencyBonus = pbSpin->value();
    entry.passivePerception = passivePerceptionLabel->text().toInt();
    entry.armorSource = armorSourceCombo ? armorSourceCombo->currentText().trimmed() : QString();
    entry.shieldSource = shieldSourceCombo ? shieldSourceCombo->currentText().trimmed() : QString();
    entry.armorDescription = acTypeEdit->text().trimmed();
    entry.hitDiceFormula = hitDiceEdit->text().trimmed();
    entry.speedDescription = speedDetailsEdit->text().trimmed();
    entry.senses = sensesEdit->text().trimmed();
    entry.damageVulnerabilities = vulnerabilitiesEdit->text().trimmed();
    entry.damageResistances = resistancesEdit->text().trimmed();
    entry.damageImmunities = immunitiesEdit->text().trimmed();
    entry.conditionImmunities = conditionImmunitiesEdit->text().trimmed();

    entry.age = ageEdit->text().trimmed();
    entry.height = heightEdit->text().trimmed();
    entry.weight = weightEdit->text().trimmed();
    entry.skin = skinEdit->text().trimmed();
    entry.hair = hairEdit->text().trimmed();
    entry.appearance = appearanceEdit->toPlainText().trimmed();
    entry.description = descriptionEdit->toPlainText().trimmed();
    entry.notes = notesEdit->toPlainText().trimmed();

    auto linesFromEdit = [](QPlainTextEdit *edit) {
        QStringList lines;
        QString content = edit->toPlainText();
        content.replace(QLatin1Char(','), QLatin1Char('\n'));
        for (const QString &line : content.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty()) {
                lines << trimmed;
            }
        }
        return lines;
    };

    entry.languages = linesFromEdit(languagesEdit);
    entry.skillProficiencies = linesFromEdit(skillsEdit);
    entry.savingThrowProficiencies = linesFromEdit(savesEdit);
    entry.armorProficiencies = linesFromEdit(armorEdit);
    entry.weaponProficiencies = linesFromEdit(weaponsEdit);

    entry.attacks.clear();
    for (int index = 0; index < attacksList->count(); ++index) {
        const QString text = attacksList->item(index)->text().trimmed();
        if (!text.isEmpty()) {
            entry.attacks << text;
        }
    }

    entry.traits.clear();
    for (int index = 0; index < traitsList->count(); ++index) {
        const QString text = traitsList->item(index)->text().trimmed();
        const int separator = text.indexOf(QStringLiteral(": "));
        if (separator > 0) {
            entry.traits.insert(text.left(separator).trimmed(), text.mid(separator + 2).trimmed());
        } else if (!text.isEmpty()) {
            entry.traits.insert(text, QString());
        }
    }

    entry.spells.clear();
    for (int index = 0; index < spellsList->count(); ++index) {
        const QString spellName = spellsList->item(index)->data(Qt::UserRole).toString().trimmed();
        if (!spellName.isEmpty()) {
            Spell spell;
            spell.name = spellName;
            spell.level = spellsList->item(index)->data(Qt::UserRole + 1).toInt();
            entry.spells << spell;
        }
    }

    entry.inventory.clear();
    for (int index = 0; index < inventoryList->count(); ++index) {
        Item item;
        item.name = inventoryList->item(index)->data(Qt::UserRole).toString().trimmed();
        item.quantity = qMax(1, inventoryList->item(index)->data(Qt::UserRole + 1).toInt());
        item.description = inventoryList->item(index)->data(Qt::UserRole + 2).toString();
        if (!item.name.isEmpty()) {
            entry.inventory << item;
        }
    }

    return entry;
}

void NpcCreatorWidget::loadNpcIntoForm(const NpcEntry &entry)
{
    m_loading = true;
    QSignalBlocker blockers[] = {
        QSignalBlocker(listNameEdit),
        QSignalBlocker(nameEdit),
        QSignalBlocker(raceEdit),
        QSignalBlocker(classEdit),
        QSignalBlocker(sizeCombo),
        QSignalBlocker(typeEdit),
        QSignalBlocker(challengeEdit),
        QSignalBlocker(xpSpin),
        QSignalBlocker(alignmentCombo),
        QSignalBlocker(levelSpin),
        QSignalBlocker(strSpin),
        QSignalBlocker(dexSpin),
        QSignalBlocker(conSpin),
        QSignalBlocker(intSpin),
        QSignalBlocker(wisSpin),
        QSignalBlocker(chaSpin),
        QSignalBlocker(acSpin),
        QSignalBlocker(maxHpSpin),
        QSignalBlocker(currentHpSpin),
        QSignalBlocker(speedSpin),
        QSignalBlocker(initiativeSpin),
        QSignalBlocker(pbSpin),
        QSignalBlocker(armorSourceCombo),
        QSignalBlocker(shieldSourceCombo),
        QSignalBlocker(acTypeEdit),
        QSignalBlocker(hitDiceEdit),
        QSignalBlocker(speedDetailsEdit),
        QSignalBlocker(sensesEdit),
        QSignalBlocker(vulnerabilitiesEdit),
        QSignalBlocker(resistancesEdit),
        QSignalBlocker(immunitiesEdit),
        QSignalBlocker(conditionImmunitiesEdit),
        QSignalBlocker(appearanceEdit),
        QSignalBlocker(descriptionEdit),
        QSignalBlocker(notesEdit),
        QSignalBlocker(languagesEdit),
        QSignalBlocker(skillsEdit),
        QSignalBlocker(savesEdit),
        QSignalBlocker(armorEdit),
        QSignalBlocker(weaponsEdit)
    };
    Q_UNUSED(blockers);

    listNameEdit->setText(entry.listName);
    nameEdit->setText(entry.name);
    raceEdit->setText(entry.race);
    classEdit->setText(entry.characterClass);
    const QString normalizedSize = entry.size.trimmed().isEmpty() ? QStringLiteral("Средний") : entry.size.trimmed();
    if (sizeCombo->findText(normalizedSize) < 0) {
        sizeCombo->addItem(normalizedSize);
    }
    sizeCombo->setCurrentText(normalizedSize);
    typeEdit->setText(entry.creatureType);
    challengeEdit->setText(entry.challengeRating);
    xpSpin->setValue(entry.experienceReward);
    alignmentCombo->setCurrentText(entry.alignment);
    levelSpin->setValue(entry.level);

    strSpin->setValue(entry.strength);
    dexSpin->setValue(entry.dexterity);
    conSpin->setValue(entry.constitution);
    intSpin->setValue(entry.intelligence);
    wisSpin->setValue(entry.wisdom);
    chaSpin->setValue(entry.charisma);

    acSpin->setValue(entry.armorClass);
    maxHpSpin->setValue(entry.maxHp);
    currentHpSpin->setValue(entry.currentHp);
    speedSpin->setValue(entry.speed);
    initiativeSpin->setValue(entry.initiative);
    pbSpin->setValue(entry.proficiencyBonus);
    armorSourceCombo->setCurrentIndex(0);
    if (!entry.armorSource.trimmed().isEmpty()) {
        const int armorIndex = armorSourceCombo->findText(entry.armorSource.trimmed(), Qt::MatchExactly);
        if (armorIndex >= 0) {
            armorSourceCombo->setCurrentIndex(armorIndex);
        }
    }
    shieldSourceCombo->setCurrentIndex(0);
    if (!entry.shieldSource.trimmed().isEmpty()) {
        const int shieldIndex = shieldSourceCombo->findText(entry.shieldSource.trimmed(), Qt::MatchExactly);
        if (shieldIndex >= 0) {
            shieldSourceCombo->setCurrentIndex(shieldIndex);
        }
    }
    acTypeEdit->setText(entry.armorDescription);
    hitDiceEdit->setText(entry.hitDiceFormula);
    speedDetailsEdit->setText(entry.speedDescription);
    sensesEdit->setText(entry.senses);
    vulnerabilitiesEdit->setText(entry.damageVulnerabilities);
    resistancesEdit->setText(entry.damageResistances);
    immunitiesEdit->setText(entry.damageImmunities);
    conditionImmunitiesEdit->setText(entry.conditionImmunities);
    passivePerceptionLabel->setText(QString::number(qMax(1, entry.passivePerception)));

    ageEdit->setText(entry.age);
    heightEdit->setText(entry.height);
    weightEdit->setText(entry.weight);
    skinEdit->setText(entry.skin);
    hairEdit->setText(entry.hair);
    appearanceEdit->setPlainText(entry.appearance);
    descriptionEdit->setPlainText(entry.description);
    notesEdit->setPlainText(entry.notes);

    languagesEdit->setPlainText(entry.languages.join(QStringLiteral("\n")));
    skillsEdit->setPlainText(entry.skillProficiencies.join(QStringLiteral("\n")));
    savesEdit->setPlainText(entry.savingThrowProficiencies.join(QStringLiteral("\n")));
    armorEdit->setPlainText(entry.armorProficiencies.join(QStringLiteral("\n")));
    weaponsEdit->setPlainText(entry.weaponProficiencies.join(QStringLiteral("\n")));

    attacksList->clear();
    for (const QString &attack : entry.attacks) {
        auto *listItem = new QListWidgetItem(attack, attacksList);
        listItem->setData(Qt::UserRole + 10, attack);
    }

    traitsList->clear();
    for (auto it = entry.traits.begin(); it != entry.traits.end(); ++it) {
        const QString line = it.value().trimmed().isEmpty()
            ? it.key()
            : QStringLiteral("%1: %2").arg(it.key(), it.value());
        auto *listItem = new QListWidgetItem(line, traitsList);
        listItem->setData(Qt::UserRole, it.key());
        listItem->setData(Qt::UserRole + 1, it.value());
    }

    spellsList->clear();
    for (const Spell &spell : entry.spells) {
        auto *item = new QListWidgetItem(
            spell.level > 0
                ? QStringLiteral("%1 (ур. %2)").arg(spell.name).arg(spell.level)
                : spell.name,
            spellsList);
        item->setData(Qt::UserRole, spell.name);
        item->setData(Qt::UserRole + 1, spell.level);
    }

    inventoryList->clear();
    for (const Item &item : entry.inventory) {
        auto *listItem = new QListWidgetItem(
            item.quantity > 1
                ? QStringLiteral("%1 x%2").arg(item.name).arg(item.quantity)
                : item.name,
            inventoryList);
        listItem->setData(Qt::UserRole, item.name);
        listItem->setData(Qt::UserRole + 1, item.quantity);
        listItem->setData(Qt::UserRole + 2, item.description);
    }

    m_acManualAdjustment = 0;
    m_hpManualAdjustment = 0;
    m_loading = false;
    recalculateNpcDerivedFields();
    m_acManualAdjustment = entry.armorClass - m_lastAutoAc;
    m_hpManualAdjustment = entry.maxHp - m_lastAutoMaxHp;
    recalculateNpcDerivedFields();
    {
        QSignalBlocker blocker(currentHpSpin);
        currentHpSpin->setValue(qBound(0, entry.currentHp, maxHpSpin->value()));
    }
    updateStatblockPreview();
}

void NpcCreatorWidget::addAttack()
{
    bool ok = false;
    const QString attack = QInputDialog::getText(
        this,
        QStringLiteral("Атака"),
        QStringLiteral("Описание атаки:"),
        QLineEdit::Normal,
        QString(),
        &ok);
    if (ok && !attack.trimmed().isEmpty()) {
        auto *listItem = new QListWidgetItem(attack.trimmed(), attacksList);
        listItem->setData(Qt::UserRole + 10, attack.trimmed());
        onFormFieldChanged();
    }
}

void NpcCreatorWidget::removeAttack()
{
    delete attacksList->takeItem(attacksList->currentRow());
    onFormFieldChanged();
}

void NpcCreatorWidget::addTrait()
{
    bool ok = false;
    const QString title = QInputDialog::getText(
        this,
        QStringLiteral("Особенность"),
        QStringLiteral("Название:"),
        QLineEdit::Normal,
        QString(),
        &ok);
    if (!ok || title.trimmed().isEmpty()) {
        return;
    }
    const QString text = QInputDialog::getText(
        this,
        QStringLiteral("Особенность"),
        QStringLiteral("Описание:"),
        QLineEdit::Normal,
        QString(),
        &ok);
    if (!ok) {
        return;
    }
    auto *listItem = new QListWidgetItem(
        text.trimmed().isEmpty() ? title.trimmed() : QStringLiteral("%1: %2").arg(title.trimmed(), text.trimmed()),
        traitsList);
    listItem->setData(Qt::UserRole, title.trimmed());
    listItem->setData(Qt::UserRole + 1, text.trimmed());
    onFormFieldChanged();
}

void NpcCreatorWidget::removeTrait()
{
    delete traitsList->takeItem(traitsList->currentRow());
    onFormFieldChanged();
}

void NpcCreatorWidget::addSpell()
{
    SpellSelectDialog dialog(this);
    
    if (dialog.exec() == QDialog::Accepted) {
        const QList<Spell> selectedSpells = dialog.selectedSpells();
        
        for (const Spell &spell : selectedSpells) {
            // Check if spell is already in list
            bool alreadyExists = false;
            for (int i = 0; i < spellsList->count(); ++i) {
                QListWidgetItem *item = spellsList->item(i);
                if (item->data(Qt::UserRole).toString() == spell.name) {
                    alreadyExists = true;
                    break;
                }
            }
            
            if (!alreadyExists) {
                auto *item = new QListWidgetItem(
                    spell.level > 0 ? QStringLiteral("%1 (ур. %2)").arg(spell.name).arg(spell.level) : spell.name,
                    spellsList);
                item->setData(Qt::UserRole, spell.name);
                item->setData(Qt::UserRole + 1, spell.level);
            }
        }
        
        onFormFieldChanged();
    }
}

void NpcCreatorWidget::removeSpell()
{
    delete spellsList->takeItem(spellsList->currentRow());
    onFormFieldChanged();
}

void NpcCreatorWidget::addInventoryItem()
{
    const QList<Item> allItems = DatabaseManager::instance().getAllItems();
    QString chosenName;
    Item chosenItem;

    if (!allItems.isEmpty()) {
        QStringList names;
        QMap<QString, Item> byName;
        for (const Item &item : allItems) {
            if (item.name.trimmed().isEmpty() || byName.contains(item.name)) {
                continue;
            }
            byName.insert(item.name, item);
            names << item.name;
        }
        names.sort(Qt::CaseInsensitive);

        bool ok = false;
        chosenName = QInputDialog::getItem(
            this,
            QStringLiteral("Добавить предмет"),
            QStringLiteral("Предмет из базы (или отмена для ручного ввода):"),
            names,
            0,
            true,
            &ok);
        if (ok && byName.contains(chosenName)) {
            chosenItem = byName.value(chosenName);
        } else if (!ok) {
            return;
        }
    }

    if (chosenItem.name.trimmed().isEmpty()) {
        bool ok = false;
        chosenName = QInputDialog::getText(
            this,
            QStringLiteral("Предмет"),
            QStringLiteral("Название предмета:"),
            QLineEdit::Normal,
            QString(),
            &ok);
        if (!ok || chosenName.trimmed().isEmpty()) {
            return;
        }
        chosenItem.name = chosenName.trimmed();
    }

    bool ok = false;
    const int quantity = QInputDialog::getInt(
        this,
        QStringLiteral("Количество"),
        QStringLiteral("Количество:"),
        1,
        1,
        999,
        1,
        &ok);
    if (!ok) {
        return;
    }

    auto *item = new QListWidgetItem(
        quantity > 1
            ? QStringLiteral("%1 x%2").arg(chosenItem.name).arg(quantity)
            : chosenItem.name,
        inventoryList);
    item->setData(Qt::UserRole, chosenItem.name);
    item->setData(Qt::UserRole + 1, quantity);
    item->setData(Qt::UserRole + 2, chosenItem.description);
    onFormFieldChanged();
}

void NpcCreatorWidget::removeInventoryItem()
{
    delete inventoryList->takeItem(inventoryList->currentRow());
    onFormFieldChanged();
}
