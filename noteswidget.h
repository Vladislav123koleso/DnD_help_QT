#ifndef NOTESWIDGET_H
#define NOTESWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QMap>
#include <QTimer>
#include "note.h"

class NotesWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NotesWidget(QWidget *parent = nullptr);
    ~NotesWidget() override;

    void setStorageScope(const QString &scope);
    void loadNotes();
    void saveCurrentNote();

private slots:
    void createNewNote();
    void deleteCurrentNote();
    void onNoteSelected(QListWidgetItem *item);
    void onNoteEdited();
    void applyBold();
    void applyItalic();
    void applyUnderline();
    void changeFontSize(int size);
    void changeTextColor();
    void changeBackgroundColor();
    void persistNotesToDisk();

private:
    void setupUi();
    void setupToolbar();
    void refreshNoteList();
    void loadNote(const Note &note);
    QString getNoteDisplayName(const Note &note) const;
    void schedulePersistToDisk();
    QString storageFilePath() const;

    QListWidget *notesList;
    QTextEdit *editor;
    QPushButton *newNoteBtn;
    QPushButton *deleteNoteBtn;

    QToolButton *boldBtn;
    QToolButton *italicBtn;
    QToolButton *underlineBtn;
    QSpinBox *fontSizeSpinBox;
    QToolButton *colorBtn;
    QToolButton *bgColorBtn;

    QMap<QString, Note> notes;
    QString currentNoteId;
    bool isLoading = false;
    QString m_storageScope;
    QTimer *saveDebounceTimer = nullptr;
};

#endif // NOTESWIDGET_H
