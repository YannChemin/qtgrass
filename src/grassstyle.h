#pragma once
#include <QString>

// Dark stylesheet modelled on GRASS wxPython's dark grey theme.
// Apply with:  qApp->setStyleSheet( GrassStyle::dark() );
namespace GrassStyle
{

inline QString dark()
{
    return R"(
/* ── Base ─────────────────────────────────────────────────── */
QWidget {
    background-color: #2b2b2b;
    color: #e0e0e0;
    font-size: 9pt;
}

/* ── Menu bar ─────────────────────────────────────────────── */
QMenuBar {
    background-color: #383838;
    color: #e0e0e0;
    border-bottom: 1px solid #555;
}
QMenuBar::item { padding: 3px 8px; }
QMenuBar::item:selected { background-color: #4a7fb5; color: #fff; }
QMenuBar::item:pressed  { background-color: #3a6fa5; color: #fff; }

/* ── Menus ─────────────────────────────────────────────────── */
QMenu {
    background-color: #383838;
    color: #e0e0e0;
    border: 1px solid #555;
}
QMenu::item { padding: 4px 24px 4px 8px; }
QMenu::item:selected { background-color: #4a7fb5; color: #fff; }
QMenu::item:disabled  { color: #777; }
QMenu::separator { height: 1px; background: #555; margin: 2px 0; }

/* Scroll arrows shown when max-height is set (Qt built-in scroller) */
QMenu::item[class="QMenu-scrollarea"] { background: #383838; }
QMenuBar QMenu { }  /* keep menus within the menu bar styled */

/* ── Toolbars ─────────────────────────────────────────────── */
QToolBar {
    background-color: #383838;
    border: none;
    spacing: 1px;
    padding: 2px;
}
QToolBar::separator { width: 1px; background: #555; margin: 2px; }
QToolButton {
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 3px;
    padding: 3px 5px;
    color: #e0e0e0;
}
QToolButton:hover   { background-color: #484848; border-color: #666; }
QToolButton:pressed { background-color: #4a7fb5; border-color: #3a6fa5; }
QToolButton:checked { background-color: #3a6fa5; border-color: #2a5f95; }

/* ── Status bar ───────────────────────────────────────────── */
QStatusBar { background-color: #383838; color: #c0c0c0; }
QStatusBar::item { border: none; }

/* ── Tree and list views ──────────────────────────────────── */
QTreeView, QListView, QTableView {
    background-color: #2b2b2b;
    alternate-background-color: #303030;
    color: #e0e0e0;
    border: 1px solid #4a4a4a;
    outline: none;
}
QTreeView::item, QListView::item { padding: 2px; }
QTreeView::item:selected, QListView::item:selected {
    background-color: #4a7fb5; color: #fff;
}
QTreeView::item:hover, QListView::item:hover {
    background-color: #3a3a3a;
}
QTreeView::branch { background-color: #2b2b2b; }
QHeaderView::section {
    background-color: #383838;
    color: #e0e0e0;
    border: 1px solid #4a4a4a;
    padding: 3px;
}

/* ── Splitters ────────────────────────────────────────────── */
QSplitter::handle { background-color: #4a4a4a; }
QSplitter::handle:horizontal { width: 3px; }
QSplitter::handle:vertical   { height: 3px; }

/* ── Scrollbars ───────────────────────────────────────────── */
QScrollBar:vertical {
    background: #2b2b2b; width: 10px; margin: 0;
}
QScrollBar::handle:vertical {
    background: #5a5a5a; min-height: 20px; border-radius: 5px;
}
QScrollBar::handle:vertical:hover { background: #6a6a6a; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal {
    background: #2b2b2b; height: 10px; margin: 0;
}
QScrollBar::handle:horizontal {
    background: #5a5a5a; min-width: 20px; border-radius: 5px;
}
QScrollBar::handle:horizontal:hover { background: #6a6a6a; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

/* ── Input widgets ────────────────────────────────────────── */
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox {
    background-color: #1e1e1e;
    color: #e0e0e0;
    border: 1px solid #555;
    border-radius: 3px;
    padding: 2px 4px;
    selection-background-color: #4a7fb5;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border-color: #4a7fb5;
}

/* ── Buttons ──────────────────────────────────────────────── */
QPushButton {
    background-color: #484848;
    color: #e0e0e0;
    border: 1px solid #606060;
    border-radius: 4px;
    padding: 4px 12px;
    min-width: 60px;
}
QPushButton:hover   { background-color: #545454; border-color: #707070; }
QPushButton:pressed { background-color: #4a7fb5; border-color: #3a6fa5; }
QPushButton:default { border-color: #4a7fb5; }
QPushButton:disabled { color: #666; border-color: #444; }

/* ── Combo box ────────────────────────────────────────────── */
QComboBox {
    background-color: #484848;
    color: #e0e0e0;
    border: 1px solid #606060;
    border-radius: 3px;
    padding: 2px 8px;
}
QComboBox::drop-down { border: none; }
QComboBox QAbstractItemView {
    background-color: #383838;
    color: #e0e0e0;
    selection-background-color: #4a7fb5;
    border: 1px solid #555;
}

/* ── Check / Radio ────────────────────────────────────────── */
QCheckBox, QRadioButton { color: #e0e0e0; spacing: 6px; }
QCheckBox::indicator, QRadioButton::indicator {
    width: 14px; height: 14px;
    background: #1e1e1e; border: 1px solid #666; border-radius: 2px;
}
QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background: #4a7fb5; border-color: #3a6fa5;
}

/* ── Tabs ─────────────────────────────────────────────────── */
QTabWidget::pane { border: 1px solid #4a4a4a; }
QTabBar::tab {
    background: #383838; color: #c0c0c0;
    border: 1px solid #4a4a4a;
    padding: 4px 10px; border-bottom: none;
}
QTabBar::tab:selected { background: #2b2b2b; color: #e0e0e0; }
QTabBar::tab:hover    { background: #404040; }

/* ── Group boxes ──────────────────────────────────────────── */
QGroupBox {
    border: 1px solid #4a4a4a;
    border-radius: 4px;
    margin-top: 8px;
    padding-top: 4px;
    color: #c0c0c0;
}
QGroupBox::title {
    subcontrol-origin: margin; subcontrol-position: top left;
    left: 8px; padding: 0 4px;
}

/* ── Dialogs ──────────────────────────────────────────────── */
QDialog { background-color: #2b2b2b; }

/* ── Message bar ──────────────────────────────────────────── */
QFrame { border: none; }

/* ── Tooltip ──────────────────────────────────────────────── */
QToolTip {
    background-color: #484848;
    color: #e0e0e0;
    border: 1px solid #666;
    padding: 2px 4px;
}

/* ── Slider ────────────────────────────────────────────────── */
QSlider::groove:horizontal {
    background: #1e1e1e; height: 4px; border-radius: 2px;
}
QSlider::handle:horizontal {
    background: #5a5a5a; width: 12px; height: 12px;
    margin: -4px 0; border-radius: 6px;
}
QSlider::handle:horizontal:hover { background: #6a6a6a; }
)";
}

} // namespace GrassStyle
