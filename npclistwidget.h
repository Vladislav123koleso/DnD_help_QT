#ifndef NPCLISTWIDGET_H
#define NPCLISTWIDGET_H

#include <QMap>
#include <QTreeWidget>
#include <QWidget>

#include "npc.h"

class QTextEdit;

class NpcListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit NpcListWidget(QWidget *parent = nullptr);

    void setStorageScope(const QString &scope);
    void reload();

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onTreeSelectionChanged();

private:
    enum class TreeItemKind { Folder, Npc };

    void rebuildTree();
    QString storageFilePath() const;

    QTreeWidget *npcTree = nullptr;
    QTextEdit *summaryView = nullptr;
    QMap<QString, NpcEntry> npcs;
    QMap<QString, NpcFolder> folders;
    QString m_storageScope;
    bool m_loading = false;

    static constexpr int RoleKind = Qt::UserRole;
    static constexpr int RoleId = Qt::UserRole + 1;
};

#endif // NPCLISTWIDGET_H
