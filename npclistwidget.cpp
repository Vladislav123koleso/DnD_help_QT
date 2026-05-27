#include "npclistwidget.h"
#include <QColor>

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>

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

} // namespace

NpcListWidget::NpcListWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *leftPanel = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 4, 8);

    auto *title = new QLabel(QStringLiteral("Созданные NPC"), leftPanel);
    title->setStyleSheet(QStringLiteral("font-weight: bold; color: #e8e8e8;"));
    leftLayout->addWidget(title);

    npcTree = new QTreeWidget(leftPanel);
    npcTree->setHeaderHidden(true);
    npcTree->setSelectionMode(QAbstractItemView::SingleSelection);
    npcTree->setStyleSheet(
        "QTreeWidget { background-color: #2b2b2b; color: #e8e8e8; border: 1px solid #3d3d3d; }"
        "QTreeWidget::item { padding: 4px; }"
        "QTreeWidget::item:selected { background-color: #404040; }");
    leftLayout->addWidget(npcTree, 1);

    auto *rightPanel = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 8, 8, 8);

    auto *summaryTitle = new QLabel(QStringLiteral("Карточка NPC"), rightPanel);
    summaryTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #e8e8e8;"));
    rightLayout->addWidget(summaryTitle);

    summaryView = new QTextEdit(rightPanel);
    summaryView->setReadOnly(true);
    summaryView->setPlaceholderText(QStringLiteral("Выберите NPC в списке слева."));
    summaryView->setStyleSheet(
        "QTextEdit { background-color: #2b2b2b; color: #e8e8e8; border: 1px solid #3d3d3d; }");
    rightLayout->addWidget(summaryView, 1);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 900});

    mainLayout->addWidget(splitter);

    connect(npcTree, &QTreeWidget::itemSelectionChanged, this, &NpcListWidget::onTreeSelectionChanged);

    setStorageScope(QStringLiteral("master_default"));
}

void NpcListWidget::setStorageScope(const QString &scope)
{
    const QString next = sanitizeStorageKey(scope);
    if (next == m_storageScope) {
        return;
    }

    m_storageScope = next;
    reload();
}

QString NpcListWidget::storageFilePath() const
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

void NpcListWidget::reload()
{
    const NpcStorageData data = NpcStorageData::loadFromFile(storageFilePath());
    folders = data.folders;
    npcs = data.npcs;
    rebuildTree();
    summaryView->clear();
}

void NpcListWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    reload();
}

void NpcListWidget::rebuildTree()
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
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    };

    for (const NpcFolder &folder : folderList) {
        auto *folderItem = new QTreeWidgetItem(npcTree, {folder.name});
        folderItem->setData(0, RoleKind, static_cast<int>(TreeItemKind::Folder));
        folderItem->setData(0, RoleId, folder.id);
        folderItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
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
            {QStringLiteral("Список пуст. Создайте NPC на вкладке «Создание NPC».")});
        hint->setFlags(Qt::NoItemFlags);
        hint->setForeground(0, QColor(QStringLiteral("#888888")));
    }

    m_loading = false;
}

void NpcListWidget::onTreeSelectionChanged()
{
    if (m_loading) {
        return;
    }

    QTreeWidgetItem *item = npcTree->currentItem();
    if (!item || static_cast<TreeItemKind>(item->data(0, RoleKind).toInt()) != TreeItemKind::Npc) {
        summaryView->clear();
        return;
    }

    const QString id = item->data(0, RoleId).toString();
    if (!npcs.contains(id)) {
        summaryView->clear();
        return;
    }

    summaryView->setPlainText(formatNpcSummary(npcs.value(id)));
}
