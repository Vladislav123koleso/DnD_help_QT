#include "itembookwidget.h"

#include "databasemanager.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSet>
#include <QSizePolicy>
#include <QSplitter>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

ItemBookWidget::ItemBookWidget(FilterMode mode, QWidget *parent)
    : QWidget(parent)
    , currentMode(mode)
{
    setObjectName(QStringLiteral("ItemBookWidget"));

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    auto *leftContainer = new QWidget(splitter);
    leftContainer->setMinimumWidth(240);
    leftContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    searchBar = new QLineEdit(leftContainer);
    searchBar->setPlaceholderText(QStringLiteral("Поиск предмета..."));
    connect(searchBar, &QLineEdit::textChanged, this, &ItemBookWidget::filterItems);

    typeFilter = new QComboBox(leftContainer);
    typeFilter->addItem(QStringLiteral("Все типы"));
    connect(typeFilter, &QComboBox::currentTextChanged, this, &ItemBookWidget::filterItems);

    rarityFilter = new QComboBox(leftContainer);
    rarityFilter->addItem(QStringLiteral("Все редкости"));
    rarityFilter->addItem(QStringLiteral("Обычный"));
    rarityFilter->addItem(QStringLiteral("Необычный"));
    rarityFilter->addItem(QStringLiteral("Редкий"));
    rarityFilter->addItem(QStringLiteral("Очень редкий"));
    rarityFilter->addItem(QStringLiteral("Легендарный"));
    connect(rarityFilter, &QComboBox::currentTextChanged, this, &ItemBookWidget::filterItems);

    filterLayout->addWidget(searchBar, 2);
    filterLayout->addWidget(typeFilter, 1);
    filterLayout->addWidget(rarityFilter, 1);
    leftLayout->addLayout(filterLayout);

    itemList = new QListWidget(leftContainer);
    connect(itemList, &QListWidget::itemClicked, this, &ItemBookWidget::onItemSelected);
    leftLayout->addWidget(itemList, 1);

    auto *detailsGroup = new QGroupBox(QStringLiteral("Детали предмета"), splitter);
    auto *detailsLayout = new QVBoxLayout(detailsGroup);

    nameLabel = new QLabel(QStringLiteral("-"), detailsGroup);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(15);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);

    typeLabel = new QLabel(QStringLiteral("Тип: -"), detailsGroup);
    rarityLabel = new QLabel(QStringLiteral("Редкость: -"), detailsGroup);
    costLabel = new QLabel(QStringLiteral("Цена: -"), detailsGroup);
    weightLabel = new QLabel(QStringLiteral("Вес: -"), detailsGroup);

    auto *infoRow = new QHBoxLayout();
    infoRow->addWidget(typeLabel);
    infoRow->addWidget(rarityLabel);

    auto *infoRow2 = new QHBoxLayout();
    infoRow2->addWidget(costLabel);
    infoRow2->addWidget(weightLabel);

    descriptionText = new QTextEdit(detailsGroup);
    descriptionText->setReadOnly(true);

    detailsLayout->addWidget(nameLabel);
    detailsLayout->addLayout(infoRow);
    detailsLayout->addLayout(infoRow2);
    detailsLayout->addWidget(new QLabel(QStringLiteral("<b>Описание:</b>"), detailsGroup));
    detailsLayout->addWidget(descriptionText, 1);

    splitter->addWidget(leftContainer);
    splitter->addWidget(detailsGroup);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({320, 960});

    mainLayout->addWidget(splitter, 1);

    loadItems();
}

void ItemBookWidget::loadItems()
{
    const QList<Item> rawItems = DatabaseManager::instance().getAllItems();
    allItems.clear();

    for (const Item &item : rawItems) {
        const bool isWeaponOrArmor =
            item.type.contains(QStringLiteral("Оружие"), Qt::CaseInsensitive) ||
            item.type.contains(QStringLiteral("Доспехи"), Qt::CaseInsensitive);

        if (currentMode == GeneralItems && isWeaponOrArmor) {
            continue;
        }
        if (currentMode == WeaponsAndArmor && !isWeaponOrArmor) {
            continue;
        }

        allItems.append(item);
    }

    std::sort(allItems.begin(), allItems.end(), [](const Item &a, const Item &b) {
        return a.name < b.name;
    });

    QSet<QString> types;
    for (const Item &item : std::as_const(allItems)) {
        if (item.type.isEmpty()) {
            continue;
        }

        const QStringList parts = item.type.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty()) {
                types.insert(trimmed);
            }
        }
    }

    QStringList sortedTypes = types.values();
    std::sort(sortedTypes.begin(), sortedTypes.end());
    for (const QString &type : std::as_const(sortedTypes)) {
        if (typeFilter->findText(type) == -1) {
            typeFilter->addItem(type);
        }
    }

    filterItems();
}

void ItemBookWidget::filterItems()
{
    itemList->clear();
    const QString search = searchBar->text().trimmed().toLower();
    const QString selectedType = typeFilter->currentText();
    const QString selectedRarity = rarityFilter->currentText();

    for (const Item &item : std::as_const(allItems)) {
        const bool matchesSearch = item.name.toLower().contains(search)
            || item.nameEng.toLower().contains(search);
        const bool matchesType =
            selectedType == QStringLiteral("Все типы")
            || item.type.contains(selectedType, Qt::CaseInsensitive);
        const bool matchesRarity =
            selectedRarity == QStringLiteral("Все редкости")
            || item.rarity == selectedRarity;

        if (matchesSearch && matchesType && matchesRarity) {
            auto *listItem = new QListWidgetItem(item.name, itemList);
            listItem->setData(Qt::UserRole, item.id);
        }
    }
}

void ItemBookWidget::onItemSelected(QListWidgetItem *listWidgetItem)
{
    if (!listWidgetItem) {
        return;
    }

    const int id = listWidgetItem->data(Qt::UserRole).toInt();
    Item selectedItem;
    for (const Item &item : std::as_const(allItems)) {
        if (item.id == id) {
            selectedItem = item;
            break;
        }
    }

    if (selectedItem.name.isEmpty()) {
        return;
    }

    nameLabel->setText(selectedItem.name + (selectedItem.nameEng.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(selectedItem.nameEng)));
    typeLabel->setText(QStringLiteral("Тип: %1").arg(selectedItem.type));
    rarityLabel->setText(QStringLiteral("Редкость: %1").arg(selectedItem.rarity));
    costLabel->setText(QStringLiteral("Цена: %1").arg(selectedItem.cost));
    weightLabel->setText(QStringLiteral("Вес: %1").arg(selectedItem.weight));
    descriptionText->setHtml(selectedItem.description);
}
