#include "MainWindow.hpp"

#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QTimer>

#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    MainWindow window;
    window.show();
    const bool hasMetrics = window.findChild<QLabel*>(QStringLiteral("feedSummary")) != nullptr;
    if (!window.isVisible() || !hasMetrics) {
        std::cerr << "GUI smoke test failed: window or metrics page was not created.\n";
        return 1;
    }
    application.processEvents();
    window.close();
    return 0;
}
