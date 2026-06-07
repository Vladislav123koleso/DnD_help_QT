#include "noteswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QLineEdit>
#include <QToolBar>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QUuid>
#include <QColorDialog>
#include <QPalette>
#include <QSizePolicy>

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

QJsonObject noteToJson(const Note &n)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"), n.id);
    o.insert(QStringLiteral("title"), n.title);
    o.insert(QStringLiteral("htmlContent"), n.htmlContent);
    o.insert(QStringLiteral("plainText"), n.plainText);
    o.insert(QStringLiteral("created"), n.created.toString(Qt::ISODateWithMs));
    o.insert(QStringLiteral("modified"), n.modified.toString(Qt::ISODateWithMs));
    return o;
}

Note noteFromJson(const QJsonObject &o)
{
    Note n;
    n.id = o.value(QStringLiteral("id")).toString();
    if (n.id.isEmpty()) {
        n.id = QUuid::createUuid().toString();
    }
    n.title = o.value(QStringLiteral("title")).toString();
    n.htmlContent = o.value(QStringLiteral("htmlContent")).toString();
    n.plainText = o.value(QStringLiteral("plainText")).toString();
    n.created = QDateTime::fromString(o.value(QStringLiteral("created")).toString(), Qt::ISODateWithMs);
    if (!n.created.isValid()) {
        n.created = QDateTime::currentDateTime();
    }
    n.modified = QDateTime::fromString(o.value(QStringLiteral("modified")).toString(), Qt::ISODateWithMs);
    if (!n.modified.isValid()) {
        n.modified = n.created;
    }
    return n;
}

} // namespace

NotesWidget::NotesWidget(QWidget *parent)
    : QWidget(parent), isLoading(false)
{
    setObjectName(QStringLiteral("NotesWidget"));
    setupUi();

    saveDebounceTimer = new QTimer(this);
    saveDebounceTimer->setSingleShot(true);
    saveDebounceTimer->setInterval(1200);
    connect(saveDebounceTimer, &QTimer::timeout, this, &NotesWidget::persistNotesToDisk);

    if (QCoreApplication *app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, [this]() {
            persistNotesToDisk();
        });
    }

    loadNotes();
}

NotesWidget::~NotesWidget()
{
    if (saveDebounceTimer) {
        saveDebounceTimer->stop();
    }
    persistNotesToDisk();
}

void NotesWidget::setStorageScope(const QString &scope)
{
    const QString next = sanitizeStorageKey(scope);
    if (next == m_storageScope) {
        return;
    }

    saveCurrentNote();
    if (saveDebounceTimer) {
        saveDebounceTimer->stop();
    }
    m_storageScope = next;
    loadNotes();
}

QString NotesWidget::storageFilePath() const
{
    if (m_storageScope.isEmpty()) {
        return QString();
    }
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(base)) {
        return QString();
    }
    return QDir(base).filePath(QStringLiteral("notes_%1.json").arg(m_storageScope));
}

void NotesWidget::persistNotesToDisk()
{
    if (m_storageScope.isEmpty()) {
        return;
    }

    if (!currentNoteId.isEmpty() && notes.contains(currentNoteId)) {
        notes[currentNoteId].htmlContent = editor->toHtml();
        notes[currentNoteId].plainText = editor->toPlainText();
        notes[currentNoteId].modified = QDateTime::currentDateTime();
        for (int i = 0; i < notesList->count(); ++i) {
            if (notesList->item(i)->data(Qt::UserRole).toString() == currentNoteId) {
                notesList->item(i)->setText(getNoteDisplayName(notes[currentNoteId]));
                break;
            }
        }
    }

    const QString path = storageFilePath();
    if (path.isEmpty()) {
        return;
    }

    QJsonArray arr;
    for (auto it = notes.constBegin(); it != notes.constEnd(); ++it) {
        arr.append(noteToJson(it.value()));
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("notes"), arr);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "NotesWidget: cannot write" << path << f.errorString();
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void NotesWidget::schedulePersistToDisk()
{
    if (saveDebounceTimer) {
        saveDebounceTimer->start();
    }
}

void NotesWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    setupToolbar();

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(8);

    QLabel *notesLabel = new QLabel(QStringLiteral("Заметки:"));
    notesLabel->setProperty("role", QStringLiteral("accent"));
    leftLayout->addWidget(notesLabel);

    notesList = new QListWidget();
    connect(notesList, &QListWidget::itemSelectionChanged, this, [this]() {
        if (!isLoading && notesList->currentItem()) {
            saveCurrentNote();
            onNoteSelected(notesList->currentItem());
        }
    });
    leftLayout->addWidget(notesList, 1);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    newNoteBtn = new QPushButton(QStringLiteral("+ Новая заметка"));
    newNoteBtn->setProperty("variant", QStringLiteral("accent"));
    connect(newNoteBtn, &QPushButton::clicked, this, &NotesWidget::createNewNote);
    buttonLayout->addWidget(newNoteBtn);

    deleteNoteBtn = new QPushButton(QStringLiteral("- Удалить"));
    deleteNoteBtn->setProperty("role", QStringLiteral("danger"));
    deleteNoteBtn->setEnabled(false);
    connect(deleteNoteBtn, &QPushButton::clicked, this, &NotesWidget::deleteCurrentNote);
    buttonLayout->addWidget(deleteNoteBtn);

    leftLayout->addLayout(buttonLayout);
    leftWidget->setMinimumWidth(220);
    leftWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    splitter->addWidget(leftWidget);

    editor = new QTextEdit();
    editor->setFont(font());
    editor->setAcceptRichText(true);
    connect(editor, &QTextEdit::textChanged, this, &NotesWidget::onNoteEdited);

    splitter->addWidget(editor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({300, 900});

    mainLayout->addWidget(splitter, 1);
}
void NotesWidget::setupToolbar()
{
    QToolBar *toolbar = new QToolBar();
    toolbar->setIconSize(QSize(18, 18));

    boldBtn = new QToolButton();
    boldBtn->setText(QStringLiteral("B"));
    boldBtn->setFont(QFont(QStringLiteral("Arial"), 10, QFont::Bold));
    boldBtn->setToolTip(QStringLiteral("Жирный (Ctrl+B)"));
    boldBtn->setCheckable(true);
    connect(boldBtn, &QToolButton::clicked, this, &NotesWidget::applyBold);
    toolbar->addWidget(boldBtn);

    italicBtn = new QToolButton();
    italicBtn->setText(QStringLiteral("I"));
    QFont italicFont = italicBtn->font();
    italicFont.setItalic(true);
    italicBtn->setFont(italicFont);
    italicBtn->setToolTip(QStringLiteral("Курсив (Ctrl+I)"));
    italicBtn->setCheckable(true);
    connect(italicBtn, &QToolButton::clicked, this, &NotesWidget::applyItalic);
    toolbar->addWidget(italicBtn);

    underlineBtn = new QToolButton();
    underlineBtn->setText(QStringLiteral("U"));
    QFont underlineFont = underlineBtn->font();
    underlineFont.setUnderline(true);
    underlineBtn->setFont(underlineFont);
    underlineBtn->setToolTip(QStringLiteral("Подчеркивание (Ctrl+U)"));
    underlineBtn->setCheckable(true);
    connect(underlineBtn, &QToolButton::clicked, this, &NotesWidget::applyUnderline);
    toolbar->addWidget(underlineBtn);

    toolbar->addSeparator();

    fontSizeSpinBox = new QSpinBox();
    fontSizeSpinBox->setMinimum(8);
    fontSizeSpinBox->setMaximum(72);
    fontSizeSpinBox->setValue(11);
    fontSizeSpinBox->setMinimumWidth(82);
    fontSizeSpinBox->setMaximumWidth(110);
    fontSizeSpinBox->setToolTip(QStringLiteral("Размер шрифта"));
    connect(fontSizeSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this, &NotesWidget::changeFontSize);
    toolbar->addWidget(fontSizeSpinBox);

    toolbar->addSeparator();

    colorBtn = new QToolButton();
    colorBtn->setText(QStringLiteral("A"));
    colorBtn->setToolTip(QStringLiteral("Цвет текста"));
    QFont colorFont(QStringLiteral("Arial"), 10, QFont::Bold);
    colorBtn->setFont(colorFont);
    connect(colorBtn, &QToolButton::clicked, this, &NotesWidget::changeTextColor);
    toolbar->addWidget(colorBtn);

    bgColorBtn = new QToolButton();
    bgColorBtn->setText(QStringLiteral("▨"));
    bgColorBtn->setToolTip(QStringLiteral("Цвет выделения"));
    connect(bgColorBtn, &QToolButton::clicked, this, &NotesWidget::changeBackgroundColor);
    toolbar->addWidget(bgColorBtn);

    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(this->layout());
    if (mainLayout) {
        mainLayout->insertWidget(0, toolbar);
    }
}
void NotesWidget::createNewNote()
{
    bool ok;
    QString title = QInputDialog::getText(this, "Новая заметка", "Название заметки:",
                                          QLineEdit::Normal, "Заметка", &ok);
    if (!ok || title.isEmpty()) {
        return;
    }

    saveCurrentNote();

    Note newNote(title, "");
    notes[newNote.id] = newNote;
    currentNoteId = newNote.id;

    refreshNoteList();

    // Выделяем новую заметку
    for (int i = 0; i < notesList->count(); ++i) {
        if (notesList->item(i)->data(Qt::UserRole).toString() == currentNoteId) {
            notesList->setCurrentRow(i);
            break;
        }
    }

    editor->clear();
    editor->setFocus();
    persistNotesToDisk();
}

void NotesWidget::deleteCurrentNote()
{
    if (currentNoteId.isEmpty()) {
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Удалить заметку?",
                                                               "Вы уверены, что хотите удалить эту заметку?",
                                                               QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

    notes.remove(currentNoteId);
    currentNoteId.clear();
    editor->clear();

    refreshNoteList();

    // Выбираем первую заметку, если она есть
    if (notesList->count() > 0) {
        notesList->setCurrentRow(0);
    } else {
        deleteNoteBtn->setEnabled(false);
    }

    persistNotesToDisk();
}

void NotesWidget::onNoteSelected(QListWidgetItem *item)
{
    QString noteId = item->data(Qt::UserRole).toString();
    if (notes.contains(noteId)) {
        currentNoteId = noteId;
        loadNote(notes[noteId]);
        deleteNoteBtn->setEnabled(true);
    }
}

void NotesWidget::loadNote(const Note &note)
{
    isLoading = true;

    // Устанавливаем HTML контент
    editor->setHtml(note.htmlContent.isEmpty() ? "" : note.htmlContent);

    // Обновляем состояние кнопок форматирования на основе текущего формата
    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt = cursor.charFormat();

    boldBtn->setChecked(fmt.fontWeight() == QFont::Bold);
    italicBtn->setChecked(fmt.fontItalic());
    underlineBtn->setChecked(fmt.fontUnderline());
    {
        const QSignalBlocker blocker(fontSizeSpinBox);
        fontSizeSpinBox->setValue(fmt.fontPointSize() > 0 ? qRound(fmt.fontPointSize()) : 11);
    }

    isLoading = false;
}

void NotesWidget::onNoteEdited()
{
    if (isLoading || currentNoteId.isEmpty()) {
        return;
    }

    if (notes.contains(currentNoteId)) {
        notes[currentNoteId].htmlContent = editor->toHtml();
        notes[currentNoteId].plainText = editor->toPlainText();
        notes[currentNoteId].modified = QDateTime::currentDateTime();
    }
    schedulePersistToDisk();
}

void NotesWidget::applyBold()
{
    if (!editor->hasFocus()) {
        editor->setFocus();
    }

    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt;
    fmt.setFontWeight(boldBtn->isChecked() ? QFont::Bold : QFont::Normal);
    cursor.mergeCharFormat(fmt);
    editor->mergeCurrentCharFormat(fmt);
}

void NotesWidget::applyItalic()
{
    if (!editor->hasFocus()) {
        editor->setFocus();
    }

    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt;
    fmt.setFontItalic(italicBtn->isChecked());
    cursor.mergeCharFormat(fmt);
    editor->mergeCurrentCharFormat(fmt);
}

void NotesWidget::applyUnderline()
{
    if (!editor->hasFocus()) {
        editor->setFocus();
    }

    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt;
    fmt.setFontUnderline(underlineBtn->isChecked());
    cursor.mergeCharFormat(fmt);
    editor->mergeCurrentCharFormat(fmt);
}

void NotesWidget::changeFontSize(int size)
{
    if (isLoading) {
        return;
    }

    editor->setFocus();
    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt;
    fmt.setFontPointSize(static_cast<qreal>(size));
    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(fmt);
    } else {
        editor->mergeCurrentCharFormat(fmt);
    }
}

void NotesWidget::changeTextColor()
{
    editor->setFocus();

    const QTextCharFormat curFmt = editor->textCursor().charFormat();
    QColor initial = curFmt.foreground().color();
    if (!initial.isValid()) {
        initial = editor->palette().color(QPalette::Text);
    }

    const QColor color = QColorDialog::getColor(initial, this, QStringLiteral("Цвет текста"));
    if (!color.isValid()) {
        return;
    }

    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt;
    fmt.setForeground(color);
    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(fmt);
    } else {
        editor->mergeCurrentCharFormat(fmt);
    }

    colorBtn->setStyleSheet(QString(
                                "QToolButton { background-color: %1; color: white; border: 1px solid #888; "
                                "border-radius: 4px; padding: 6px 10px; min-width: 28px; }"
                                "QToolButton:hover { border-color: #aaa; }")
                                .arg(color.name()));
}

void NotesWidget::changeBackgroundColor()
{
    editor->setFocus();

    const QTextCharFormat curFmt = editor->textCursor().charFormat();
    QColor initial = curFmt.background().color();
    if (!initial.isValid()) {
        initial = QColor(255, 255, 180);
    }

    const QColor color = QColorDialog::getColor(initial, this, QStringLiteral("Цвет выделения"));
    if (!color.isValid()) {
        return;
    }

    QTextCursor cursor = editor->textCursor();
    QTextCharFormat fmt;
    fmt.setBackground(color);
    if (cursor.hasSelection()) {
        cursor.mergeCharFormat(fmt);
    } else {
        editor->mergeCurrentCharFormat(fmt);
    }

    bgColorBtn->setStyleSheet(QString(
                                  "QToolButton { background-color: %1; color: white; border: 1px solid #888; "
                                  "border-radius: 4px; padding: 6px 10px; min-width: 28px; }"
                                  "QToolButton:hover { border-color: #aaa; }")
                                  .arg(color.name()));
}

void NotesWidget::refreshNoteList()
{
    isLoading = true;
    notesList->clear();

    for (auto it = notes.begin(); it != notes.end(); ++it) {
        QListWidgetItem *item = new QListWidgetItem(getNoteDisplayName(it.value()));
        item->setData(Qt::UserRole, it.key());
        notesList->addItem(item);
    }

    isLoading = false;
}

QString NotesWidget::getNoteDisplayName(const Note &note) const
{
    QString display = note.title;
    if (!note.modified.isNull()) {
        display += QString(" (%1)").arg(note.modified.toString("dd.MM.yyyy hh:mm"));
    }
    return display;
}

void NotesWidget::loadNotes()
{
    if (saveDebounceTimer) {
        saveDebounceTimer->stop();
    }

    notes.clear();
    currentNoteId.clear();
    editor->clear();

    if (m_storageScope.isEmpty()) {
        refreshNoteList();
        deleteNoteBtn->setEnabled(false);
        return;
    }

    const QString path = storageFilePath();
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        refreshNoteList();
        deleteNoteBtn->setEnabled(false);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        refreshNoteList();
        deleteNoteBtn->setEnabled(false);
        return;
    }

    const QJsonArray arr = doc.object().value(QStringLiteral("notes")).toArray();
    for (const QJsonValue &v : arr) {
        if (!v.isObject()) {
            continue;
        }
        const Note n = noteFromJson(v.toObject());
        if (!n.id.isEmpty()) {
            notes.insert(n.id, n);
        }
    }

    refreshNoteList();
    if (notesList->count() > 0) {
        notesList->setCurrentRow(0);
    } else {
        deleteNoteBtn->setEnabled(false);
    }
}

void NotesWidget::saveCurrentNote()
{
    if (!currentNoteId.isEmpty() && notes.contains(currentNoteId)) {
        notes[currentNoteId].htmlContent = editor->toHtml();
        notes[currentNoteId].plainText = editor->toPlainText();
        notes[currentNoteId].modified = QDateTime::currentDateTime();
        
        // Обновляем отображение в списке
        for (int i = 0; i < notesList->count(); ++i) {
            if (notesList->item(i)->data(Qt::UserRole).toString() == currentNoteId) {
                notesList->item(i)->setText(getNoteDisplayName(notes[currentNoteId]));
                break;
            }
        }
    }
    persistNotesToDisk();
}

