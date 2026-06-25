#include "apptheme.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QStringLiteral>

namespace AppTheme {

void apply(QApplication &app)
{
    app.setStyle(QStringLiteral("Fusion"));
    QFont appFont(QStringLiteral("Segoe UI Variable"), 10);
    appFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(appFont);

    const QColor baseBackground(QStringLiteral("#eae0d5"));
    const QColor surface(QStringLiteral("#f6efe6"));
    const QColor alternateSurface(QStringLiteral("#ddd0c0"));
    const QColor panelDark(QStringLiteral("#22333b"));
    const QColor textPrimary(QStringLiteral("#0a0908"));
    const QColor textMuted(QStringLiteral("#5e503f"));
    const QColor accentRed(QStringLiteral("#9d3a34"));

    QPalette palette;
    palette.setColor(QPalette::Window, baseBackground);
    palette.setColor(QPalette::WindowText, textPrimary);
    palette.setColor(QPalette::Base, surface);
    palette.setColor(QPalette::AlternateBase, alternateSurface);
    palette.setColor(QPalette::Text, textPrimary);
    palette.setColor(QPalette::Button, surface);
    palette.setColor(QPalette::ButtonText, textPrimary);
    palette.setColor(QPalette::Highlight, panelDark);
    palette.setColor(QPalette::HighlightedText, baseBackground);
    palette.setColor(QPalette::ToolTipBase, surface);
    palette.setColor(QPalette::ToolTipText, textPrimary);
    palette.setColor(QPalette::PlaceholderText, textMuted);
    palette.setColor(QPalette::Link, accentRed);
    palette.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#7f302b")));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QStringLiteral("#94877a")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#94877a")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#94877a")));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(QStringLiteral("#e2d8cd")));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(QStringLiteral("#e2d8cd")));
    app.setPalette(palette);

    app.setStyleSheet(QStringLiteral(R"(
QMainWindow,
QDialog,
QMessageBox,
QStackedWidget,
QWidget#centralwidget,
QWidget#PlayerPage,
QWidget#CharacterSheet {
    background-color: #eae0d5;
    color: #0a0908;
}

QWidget {
    color: #0a0908;
    selection-background-color: #22333b;
    selection-color: #f8f2e8;
}

QFrame#Card,
QGroupBox,
QMenu {
    background-color: #fffaf5;
    border: 1px solid #c6ac8f;
    border-radius: 14px;
}

QGroupBox {
    margin-top: 18px;
    padding: 16px 14px 14px 14px;
    font-weight: 700;
    color: #22333b;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 14px;
    padding: 1px 8px;
    color: #22333b;
    background-color: #eae0d5;
}

QTabWidget::pane {
    border: none;
    background-color: transparent;
    top: 0px;
}

QTabBar::tab {
    background-color: #d8c7b6;
    color: #22333b;
    border: 1px solid #c6ac8f;
    border-radius: 10px;
    padding: 8px 14px;
    margin-right: 5px;
    min-width: 108px;
    font-weight: 700;
}

QTabBar::tab:selected {
    background-color: #22333b;
    color: #f8f2e8;
    border-color: #22333b;
}

QTabBar::tab:hover:!selected {
    background-color: #cbb59d;
    border-color: #b79673;
}

QLineEdit,
QTextEdit,
QPlainTextEdit,
QSpinBox,
QComboBox,
QListWidget,
QTreeWidget {
    background-color: #fffaf5;
    border: 1px solid #c6ac8f;
    border-radius: 10px;
    color: #0a0908;
    padding: 7px 9px;
    selection-background-color: #22333b;
    selection-color: #fffaf5;
}

QLineEdit,
QSpinBox,
QComboBox {
    min-height: 24px;
}

QLineEdit:read-only {
    background-color: #f3eadf;
    border-color: #d7c4af;
    color: #1d2529;
}

QComboBox {
    padding-right: 36px;
}

QTextEdit:read-only,
QPlainTextEdit:read-only {
    background-color: #f3e8dc;
}

QLineEdit:focus,
QTextEdit:focus,
QPlainTextEdit:focus,
QSpinBox:focus,
QComboBox:focus,
QListWidget:focus,
QTreeWidget:focus {
    border: 1px solid #22333b;
    background-color: #fffdf9;
}

QComboBox::drop-down {
    subcontrol-origin: border;
    subcontrol-position: top right;
    width: 30px;
    border-left: 1px solid #c6ac8f;
    border-top-right-radius: 10px;
    border-bottom-right-radius: 10px;
    background-color: #eadfd1;
}

QComboBox::drop-down:hover {
    background-color: #e0d2c3;
}

QComboBox::down-arrow {
    image: url(:/DndHelperDesign/DndHelperDesignContent/images/ui/combo_down_arrow.png);
    width: 16px;
    height: 16px;
}

QComboBox::down-arrow:on {
    top: 1px;
}

QComboBox QAbstractItemView {
    background-color: #fffaf5;
    border: 1px solid #c6ac8f;
    border-radius: 10px;
    padding: 5px;
    outline: 0;
    selection-background-color: #22333b;
    selection-color: #fffaf5;
}

QPushButton,
QToolButton {
    background-color: #22333b;
    color: #f8f2e8;
    border: 1px solid #22333b;
    border-radius: 10px;
    padding: 8px 14px;
    font-weight: 700;
    min-height: 20px;
}

QPushButton:hover,
QToolButton:hover {
    background-color: #2f434d;
    border-color: #2f434d;
}

QPushButton:pressed,
QToolButton:pressed {
    background-color: #1a262c;
    border-color: #1a262c;
}

QPushButton:disabled,
QToolButton:disabled {
    background-color: #d8cfc4;
    color: #8f8173;
    border-color: #d8cfc4;
}

QPushButton[variant="accent"] {
    background-color: #9d3a34;
    border-color: #9d3a34;
    color: #fff7f2;
}

QPushButton[variant="accent"]:hover {
    background-color: #b14a44;
    border-color: #b14a44;
}

QPushButton[variant="accent"]:pressed {
    background-color: #7f2e2b;
    border-color: #7f2e2b;
}

QPushButton[role="danger"] {
    background-color: #8e2b2b;
    border-color: #8e2b2b;
    color: #fff4ef;
}

QPushButton[role="danger"]:hover {
    background-color: #aa3a3a;
    border-color: #aa3a3a;
}

QPushButton[role="danger"]:pressed {
    background-color: #732222;
    border-color: #732222;
}

QListWidget::item,
QTreeWidget::item {
    padding: 7px;
    border-radius: 8px;
}

QListWidget::item:selected,
QTreeWidget::item:selected {
    background-color: #22333b;
    color: #f8f2e8;
}

QListWidget::item:hover:!selected,
QTreeWidget::item:hover:!selected {
    background-color: #e0d2c3;
}

QHeaderView::section {
    background-color: #eadfd1;
    color: #22333b;
    border: none;
    border-right: 1px solid #c6ac8f;
    border-bottom: 1px solid #c6ac8f;
    padding: 7px 8px;
    font-weight: 700;
}

QCheckBox,
QRadioButton {
    spacing: 7px;
    color: #0a0908;
}

QCheckBox::indicator,
QRadioButton::indicator {
    width: 15px;
    height: 15px;
    border: 1px solid #9f8568;
    background-color: #fffaf5;
}

QRadioButton::indicator {
    border-radius: 8px;
}

QRadioButton::indicator:checked {
    border: 4px solid #22333b;
    background-color: #fffaf5;
}

QCheckBox::indicator {
    border-radius: 4px;
}

QCheckBox::indicator:checked {
    background-color: #22333b;
    border-color: #22333b;
}

QToolBar {
    background-color: #e3d5c6;
    border: 1px solid #c6ac8f;
    border-radius: 12px;
    spacing: 5px;
    padding: 6px;
}

QToolBar QToolButton {
    padding: 6px 10px;
}

QMenu {
    padding: 7px;
}

QMenu::item {
    padding: 8px 20px;
    border-radius: 8px;
}

QMenu::item:selected {
    background-color: #22333b;
    color: #f8f2e8;
}

QMenu::separator {
    height: 1px;
    background-color: #c6ac8f;
    margin: 6px 4px;
}

QScrollBar:vertical {
    background: #e5dacc;
    width: 11px;
    margin: 2px;
    border-radius: 7px;
}

QScrollBar::handle:vertical {
    background: #b79d80;
    min-height: 24px;
    border-radius: 7px;
}

QScrollBar::handle:vertical:hover {
    background: #9f8568;
}

QScrollBar::sub-line:vertical,
QScrollBar::add-line:vertical,
QScrollBar::sub-page:vertical,
QScrollBar::add-page:vertical {
    background: none;
    border: none;
}

QScrollBar:horizontal {
    background: #e5dacc;
    height: 11px;
    margin: 2px;
    border-radius: 7px;
}

QScrollBar::handle:horizontal {
    background: #b79d80;
    min-width: 24px;
    border-radius: 7px;
}

QScrollBar::handle:horizontal:hover {
    background: #9f8568;
}

QScrollBar::sub-line:horizontal,
QScrollBar::add-line:horizontal,
QScrollBar::sub-page:horizontal,
QScrollBar::add-page:horizontal {
    background: none;
    border: none;
}

QSplitter::handle {
    background-color: #d2c1ad;
    border-radius: 3px;
}

QSplitter::handle:horizontal {
    width: 6px;
}

QSplitter::handle:vertical {
    height: 6px;
}

QLabel[role="muted"] {
    color: #5e503f;
}

QLabel[role="accent"] {
    color: #9d3a34;
    font-weight: 700;
}

QToolTip {
    background-color: #22333b;
    color: #fffaf5;
    border: 1px solid #5e503f;
    border-radius: 8px;
    padding: 8px;
}

QWidget#CharacterSheet QGroupBox {
    background-color: #fffaf5;
}

QWidget#CharacterSheet QGroupBox#statsGroup QLabel,
QWidget#CharacterSheet QGroupBox#combatGroup QLabel {
    background-color: #eadfd1;
    color: #0a0908;
    border-radius: 6px;
    padding: 4px 4px;
    font-weight: 700;
}

QWidget#CharacterSheet QGroupBox#statsGroup QLineEdit,
QWidget#CharacterSheet QGroupBox#combatGroup QLineEdit {
    background-color: #fffdf9;
    border-color: #c6ac8f;
    font-weight: 700;
}

QWidget#CharacterSheet QGroupBox#statsGroup QLineEdit {
    border-radius: 12px;
}

QWidget#SidebarPanel {
    background-color: #22333b;
    border-right: 1px solid #182227;
}

QListWidget#SidebarNav {
    background-color: transparent;
    border: none;
    color: #f8f2e8;
}

QListWidget#SidebarNav::item {
    border-radius: 0;
    padding: 10px 12px;
    border-left: 4px solid transparent;
}

QListWidget#SidebarNav::item:selected {
    background-color: #2f434d;
    border-left: 4px solid #9d3a34;
    color: #f8f2e8;
}

QListWidget#SidebarNav::item:hover:!selected {
    background-color: #2a3b44;
}

QPushButton#SidebarToggleBtn {
    background-color: transparent;
    border: 1px solid transparent;
    color: #f8f2e8;
    border-radius: 8px;
    padding: 6px;
}

QPushButton#SidebarToggleBtn:hover {
    background-color: #2f434d;
    border-color: #2f434d;
}

QPushButton#SidebarToggleBtn:pressed {
    background-color: #1a262c;
}

QToolButton#SidebarMenuBtn {
    background-color: #9d3a34;
    border: 1px solid #9d3a34;
    color: #fff7f2;
    border-radius: 8px;
    padding: 6px;
}

QToolButton#SidebarMenuBtn:hover {
    background-color: #b14a44;
    border-color: #b14a44;
}

QToolButton#SidebarMenuBtn:pressed {
    background-color: #7f2e2b;
    border-color: #7f2e2b;
}

QToolButton#SidebarMenuBtn::menu-indicator {
    image: none;
    width: 0px;
}

QMainWindow[uxCompact="true"] QGroupBox {
    margin-top: 12px;
    padding: 8px;
}

QMainWindow[uxCompact="true"] QTabBar::tab {
    min-width: 88px;
    padding: 5px 8px;
    margin-right: 2px;
}

QMainWindow[uxCompact="true"] QLineEdit,
QMainWindow[uxCompact="true"] QTextEdit,
QMainWindow[uxCompact="true"] QPlainTextEdit,
QMainWindow[uxCompact="true"] QSpinBox,
QMainWindow[uxCompact="true"] QComboBox,
QMainWindow[uxCompact="true"] QListWidget,
QMainWindow[uxCompact="true"] QTreeWidget {
    padding: 5px 7px;
}

QMainWindow[uxCompact="true"] QPushButton,
QMainWindow[uxCompact="true"] QToolButton {
    padding: 6px 10px;
}

QMainWindow[uxCompact="true"] QToolBar {
    padding: 4px;
    spacing: 3px;
}

QMainWindow[uxTight="true"] QTabBar::tab {
    min-width: 78px;
    padding: 4px 7px;
    font-size: 9pt;
}

QMainWindow[uxTight="true"] QLineEdit,
QMainWindow[uxTight="true"] QTextEdit,
QMainWindow[uxTight="true"] QPlainTextEdit,
QMainWindow[uxTight="true"] QSpinBox,
QMainWindow[uxTight="true"] QComboBox,
QMainWindow[uxTight="true"] QListWidget,
QMainWindow[uxTight="true"] QTreeWidget {
    padding: 4px 6px;
}

QMainWindow[uxTight="true"] QPushButton,
QMainWindow[uxTight="true"] QToolButton {
    padding: 5px 8px;
    font-size: 9pt;
}

QMainWindow[uxTight="true"] QGroupBox {
    margin-top: 10px;
    padding: 6px;
}

QMainWindow[uxTight="true"] QToolBar {
    padding: 3px;
    spacing: 2px;
}
    )"));
}

} // namespace AppTheme
