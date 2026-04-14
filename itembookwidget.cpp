#include "itembookwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include "databasemanager.h"

ItemBookWidget::ItemBookWidget(FilterMode mode, QWidget *parent) : QWidget(parent), currentMode(mode)
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    // --- Left Side: Filters & List ---
    QVBoxLayout *leftLayout = new QVBoxLayout(); // Removed "this" from layout constructor to avoid parent issues

    // Filters
    QHBoxLayout *filterLayout = new QHBoxLayout();
    
    searchBar = new QLineEdit();
    searchBar->setPlaceholderText("Поиск предмета...");
    connect(searchBar, &QLineEdit::textChanged, this, &ItemBookWidget::filterItems);
    
    typeFilter = new QComboBox();
    typeFilter->addItem("Все типы");
    // Types will be populated based on loaded items in loadItems()
    connect(typeFilter, &QComboBox::currentTextChanged, this, &ItemBookWidget::filterItems);

    rarityFilter = new QComboBox();
    rarityFilter->addItem("Все редкости");
    rarityFilter->addItem("Обычный");
    rarityFilter->addItem("Необычный");
    rarityFilter->addItem("Редкий");
    rarityFilter->addItem("Очень редкий");
    rarityFilter->addItem("Легендарный");
    connect(rarityFilter, &QComboBox::currentTextChanged, this, &ItemBookWidget::filterItems);

    filterLayout->addWidget(searchBar);
    filterLayout->addWidget(typeFilter);
    filterLayout->addWidget(rarityFilter);

    leftLayout->addLayout(filterLayout);

    // List
    itemList = new QListWidget();
    connect(itemList, &QListWidget::itemClicked, this, &ItemBookWidget::onItemSelected);
    leftLayout->addWidget(itemList);

    // Обертка для левой колонки, чтобы жестко задать ширину
    QWidget *leftContainer = new QWidget();
    leftContainer->setLayout(leftLayout);
    leftContainer->setMaximumWidth(300); // Ограничиваем ширину списка

    mainLayout->addWidget(leftContainer);

    // --- Right Side: Details ---
    QGroupBox *detailsGroup = new QGroupBox("Детали предмета");
    QVBoxLayout *detailsLayout = new QVBoxLayout(detailsGroup);

    nameLabel = new QLabel("-");
    // Change color to white (as requested)
    nameLabel->setStyleSheet("font-size: 16pt; font-weight: bold; color: white;");
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);

    typeLabel = new QLabel("Тип: -");
    rarityLabel = new QLabel("Редкость: -");
    costLabel = new QLabel("Цена: -");
    weightLabel = new QLabel("Вес: -");

    QHBoxLayout *infoRow = new QHBoxLayout();
    infoRow->addWidget(typeLabel);
    infoRow->addWidget(rarityLabel);
    
    QHBoxLayout *infoRow2 = new QHBoxLayout();
    infoRow2->addWidget(costLabel);
    infoRow2->addWidget(weightLabel);

    descriptionText = new QTextEdit();
    descriptionText->setReadOnly(true);

    detailsLayout->addWidget(nameLabel);
    detailsLayout->addLayout(infoRow);
    detailsLayout->addLayout(infoRow2);
    detailsLayout->addWidget(new QLabel("<b>Описание:</b>"));
    detailsLayout->addWidget(descriptionText);

    mainLayout->addWidget(detailsGroup, 5); // Stretch modified to makes left column smaller

    // Load Data
    loadItems();
}

void ItemBookWidget::loadItems()
{
    QList<Item> rawItems = DatabaseManager::instance().getAllItems();
    allItems.clear();

    // Filter by Mode immediately
    for(const auto& item : rawItems) {
        bool isWeaponOrArmor = item.type.contains("Оружие", Qt::CaseInsensitive) || item.type.contains("Доспехи", Qt::CaseInsensitive);

        if (currentMode == GeneralItems && isWeaponOrArmor) continue;
        if (currentMode == WeaponsAndArmor && !isWeaponOrArmor) continue;

        allItems.append(item);
    }
    
    // Сортировка по алфавиту
    std::sort(allItems.begin(), allItems.end(), [](const Item &a, const Item &b) {
        return a.name < b.name;
    });
    
    // Populate Types logic
    QSet<QString> types;
    for(const auto& item : allItems) {
        if(item.type.isEmpty()) continue;
        
        // Split types by comma to get individual tags (e.g. "Weapon", "Melee")
        QStringList parts = item.type.split(',', Qt::SkipEmptyParts);
        for(const QString &part : parts) {
             types.insert(part.trimmed());
        }
    }
    
    // Sort types alphabetically for better UX
    QStringList sortedTypes = types.values();
    std::sort(sortedTypes.begin(), sortedTypes.end());

    for(const QString& t : sortedTypes) {
        if(typeFilter->findText(t) == -1) {
            typeFilter->addItem(t);
        }
    }

    filterItems();
}

void ItemBookWidget::filterItems()
{
    itemList->clear();
    QString search = searchBar->text().toLower();
    QString typeSel = typeFilter->currentText();
    QString raritySel = rarityFilter->currentText();

    for (const Item &item : allItems) {
        bool matchSearch = item.name.toLower().contains(search) || item.nameEng.toLower().contains(search);
        
        bool matchType = (typeSel == "Все типы") || item.type.contains(typeSel, Qt::CaseInsensitive); // Contains allow partial match
        bool matchRarity = (raritySel == "Все редкости") || (item.rarity == raritySel);

        if (matchSearch && matchType && matchRarity) {
            QListWidgetItem *listItem = new QListWidgetItem(item.name);
            // Store index or ID in data? simpler to just store pointer or find by name.
            // Let's store ID if we had it, or just use the index in "filtered" list?
            // Safer: Store ID in UserRole
            listItem->setData(Qt::UserRole, item.id);
            itemList->addItem(listItem);
        }
    }
}

void ItemBookWidget::onItemSelected(QListWidgetItem *listWidgetItem)
{
    int id = listWidgetItem->data(Qt::UserRole).toInt();
    
    // Find item by ID
    Item selectedItem;
    for(const auto& item : allItems) {
        if(item.id == id) {
            selectedItem = item;
            break;
        }
    }

    if (selectedItem.name.isEmpty()) return; // Not found?

    nameLabel->setText(selectedItem.name + (selectedItem.nameEng.isEmpty() ? "" : " (" + selectedItem.nameEng + ")"));
    typeLabel->setText("Тип: " + selectedItem.type);
    rarityLabel->setText("Редкость: " + selectedItem.rarity);
    costLabel->setText("Цена: " + selectedItem.cost);
    weightLabel->setText("Вес: " + selectedItem.weight);
    descriptionText->setHtml(selectedItem.description);
}
