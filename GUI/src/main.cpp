#include "MainWindow.hpp"

#include <QApplication>
#include <QColor>
#include <QPalette>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Limit Order Book");
    app.setOrganizationName("LimitOrderBookMatchingEngine");

    app.setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#1e1e2e"));
    palette.setColor(QPalette::WindowText, QColor("#cdd6f4"));
    palette.setColor(QPalette::Base, QColor("#181825"));
    palette.setColor(QPalette::AlternateBase, QColor("#313244"));
    palette.setColor(QPalette::Text, QColor("#cdd6f4"));
    palette.setColor(QPalette::Button, QColor("#313244"));
    palette.setColor(QPalette::ButtonText, QColor("#cdd6f4"));
    palette.setColor(QPalette::Highlight, QColor("#89b4fa"));
    palette.setColor(QPalette::HighlightedText, QColor("#1e1e2e"));
    app.setPalette(palette);
    app.setStyleSheet(
        "QMainWindow, QWidget { background: #1e1e2e; color: #cdd6f4; }"
        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTableWidget, QTreeWidget "
        "{ background: #181825; color: #cdd6f4; border: 1px solid #45475a; "
        "selection-background-color: #45475a; }"
        "QComboBox QAbstractItemView { background: #181825; color: #cdd6f4; "
        "selection-background-color: #45475a; }"
        "QPushButton { background: #313244; border: 1px solid #585b70; "
        "padding: 5px 9px; border-radius: 4px; }"
        "QPushButton:hover { background: #45475a; }"
        "QPushButton#navButton { padding: 8px 16px; border: 1px solid transparent; "
        "background: transparent; color: #a6adc8; }"
        "QPushButton#navButton:hover { background: #313244; color: #cdd6f4; }"
        "QPushButton#navButton:checked { background: #313244; color: #89b4fa; "
        "border: 1px solid #585b70; }"
        "QLabel#brandLabel { color: #89b4fa; font-weight: 700; letter-spacing: 1px; }"
        "QLabel#pageTitle { color: #cdd6f4; font-size: 20px; font-weight: 700; }"
        "QLabel#sectionTitle { color: #cdd6f4; font-size: 16px; font-weight: 600; }"
        "QLabel#mutedText { color: #a6adc8; }"
        "QLabel#feedSummary { background: #181825; color: #94e2d5; "
        "border: 1px solid #45475a; border-radius: 6px; padding: 10px 14px; }"
        "QLabel#statusBanner { background: #181825; color: #cdd6f4; "
        "border: 1px solid #45475a; border-radius: 6px; padding: 10px 14px; }"
        "QFrame#metricCard, QFrame#panelCard { background: #181825; "
        "border: 1px solid #45475a; border-radius: 8px; }"
        "QLabel#metricValue { font-size: 22px; font-weight: 700; }"
        "QPushButton#primaryButton { background: #45475a; border: 1px solid #89b4fa; "
        "color: #cdd6f4; font-weight: 600; }"
        "QPushButton#primaryButton:hover { background: #585b70; }"
        "QHeaderView::section { background: #313244; color: #cdd6f4; padding: 4px; }"
        "QTabWidget::pane { border: 1px solid #45475a; top: -1px; }"
        "QTabBar::tab { background: #181825; color: #a6adc8; padding: 8px 16px; "
        "border: 1px solid #313244; }"
        "QTabBar::tab:selected { background: #313244; color: #89b4fa; }"
        "QGraphicsView { border: 1px solid #45475a; }"
        "QScrollBar:vertical, QScrollBar:horizontal { background: #181825; "
        "border: none; margin: 0; }"
        "QScrollBar::handle:vertical, QScrollBar::handle:horizontal { "
        "background: #45475a; border-radius: 4px; min-height: 24px; min-width: 24px; }"
        "QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover "
        "{ background: #585b70; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }"
        "QToolTip { background: #313244; color: #cdd6f4; border: 1px solid #585b70; }"
        "QProgressBar { border: 1px solid #45475a; text-align: center; }"
        "QProgressBar::chunk { background: #89b4fa; }"
    );

    MainWindow window;
    window.showMaximized();
    return app.exec();
}
