#include "MainWindow.hpp"

#include <qcustomplot.h>

#include <QApplication>
#include <QDebug>
#include <QEventLoop>
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
    auto* latencyPlot = window.findChild<QCustomPlot*>(QStringLiteral("latencyPlot"));
    auto* activityPlot = window.findChild<QCustomPlot*>(QStringLiteral("activityPlot"));
    if (!latencyPlot || !activityPlot) {
        std::cerr << "GUI smoke test failed: metric charts were not created.\n";
        return 1;
    }

    latencyPlot->mouseWheel(nullptr);
    activityPlot->mousePress(nullptr);
    latencyPlot->xAxis->setRange(2, 8);
    latencyPlot->yAxis->setRange(0.2, 0.8);
    activityPlot->xAxis->setRange(3, 9);
    activityPlot->yAxis->setRange(4, 10);
    QEventLoop waitForDashboardUpdate;
    QTimer::singleShot(1100, &waitForDashboardUpdate, &QEventLoop::quit);
    waitForDashboardUpdate.exec();

    if (latencyPlot->xAxis->range().lower != 2 ||
        latencyPlot->xAxis->range().upper != 8 ||
        latencyPlot->yAxis->range().lower != 0.2 ||
        latencyPlot->yAxis->range().upper != 0.8 ||
        activityPlot->xAxis->range().lower != 3 ||
        activityPlot->xAxis->range().upper != 9 ||
        activityPlot->yAxis->range().lower != 4 ||
        activityPlot->yAxis->range().upper != 10) {
        std::cerr << "GUI smoke test failed: chart zoom did not persist across dashboard updates.\n";
        return 1;
    }
    application.processEvents();
    window.close();
    return 0;
}
