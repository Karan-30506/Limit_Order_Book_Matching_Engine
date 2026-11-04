#ifndef GUI_MAIN_WINDOW_HPP
#define GUI_MAIN_WINDOW_HPP

#include <QMainWindow>

#include <chrono>
#include <cstdint>
#include <vector>

#include "EngineWorker.hpp"

class QColor;
class QComboBox;
class QGraphicsView;
class QGraphicsScene;
class QLabel;
class QProgressBar;
class QPushButton;
class QThread;
class QStackedWidget;
class QString;
class QSpinBox;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QTimer;
class QCustomPlot;
class QTabWidget;
class TreeGraphicsView;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    void createNavigation();
    QWidget* createOrderPanel();
    QWidget* createQueuePanel();
    QWidget* createReplayPanel();
    QWidget* createCsvPanel();
    QWidget* createMetricsPanel();
    QWidget* createBookPanel();
    void resetSession();
    void submitOrder();
    void cancelOrder();
    void modifyOrder();
    void handleEngineReply(const EngineReply& reply);
    void applyBookSnapshot(const BookSnapshot& snapshot);
    void setOrderActionsEnabled(bool enabled);
    void zoomBookView(double factor);
    void fitBookView();
    void chooseOrderFile();
    void loadOrders(const QString& path);
    void processLoadedBatch();
    void refreshBook();
    void refreshQueue(QTreeWidget* tree, QTreeWidgetItem* item);
    void refreshQueue(Side side, int price);
    void appendLevel(QTreeWidget* tree, QTreeWidgetItem* parent,
                     const std::vector<PriceLevelSnapshot>& levels, int index);
    const PriceLevelSnapshot* findLevel(
        const std::vector<PriceLevelSnapshot>& levels, int price) const;
    double drawBookTree(QGraphicsScene* scene,
                        const std::vector<PriceLevelSnapshot>& levels, int root,
                        double left, const QColor& color, Side side);
    void appendReplayStep();
    void chooseReplayFile();
    void loadReplay(const QString& path);
    void readReplayFile(const QString& path);
    void validateReplayAgainstLive();
    void updateReplayView(const ReplaySnapshot& snapshot);
    void chooseCsvFile();
    void loadCsv(const QString& path);
    void recordMetric(double latencyMs);
    void updateMetricsDashboard();
    void showError(const QString& message);

    EngineWorker* engineWorker_ = nullptr;
    QThread* engineThread_ = nullptr;
    BookSnapshot bookSnapshot_;
    std::vector<OrderCommand> pendingOrders_;
    std::size_t nextOrderIndex_ = 0;
    std::size_t replayCursor_ = 0;
    std::size_t replayEventCount_ = 0;
    std::uint64_t nextTimestamp_ = 1;
    bool fullSystemMode_ = false;
    bool engineReady_ = false;
    bool engineRequestPending_ = false;
    bool batchInFlight_ = false;
    bool batchCancelRequested_ = false;
    std::uint64_t nextRequestID_ = 1;
    std::uint64_t resetRequestID_ = 0;
    std::uint64_t replayValidationRequestID_ = 0;
    std::uint64_t replayFlushRequestID_ = 0;
    std::uint64_t replayLoadRequestID_ = 0;
    std::uint64_t replayStepRequestID_ = 0;
    std::uint64_t orderParseRequestID_ = 0;
    QString pendingReplayPath_;
    std::chrono::steady_clock::time_point sessionStart_;
    std::size_t submissionCalls_ = 0;
    std::size_t acceptedOrders_ = 0;
    std::size_t displayedTradeCount_ = 0;

    QTreeWidget* buyTree_ = nullptr;
    QTreeWidget* sellTree_ = nullptr;
    QTableWidget* queueTable_ = nullptr;
    QTableWidget* ordersTable_ = nullptr;
    QTableWidget* tradesTable_ = nullptr;
    QTableWidget* replayTable_ = nullptr;
    QTableWidget* eventTable_ = nullptr;
    QTableWidget* csvTable_ = nullptr;
    QTableWidget* orderPageOrdersTable_ = nullptr;
    QTableWidget* orderPageTradesTable_ = nullptr;
    QTableWidget* orderHistoryTable_ = nullptr;
    QComboBox* modeCombo_ = nullptr;
    QComboBox* sideCombo_ = nullptr;
    QComboBox* typeCombo_ = nullptr;
    QSpinBox* idInput_ = nullptr;
    QSpinBox* priceInput_ = nullptr;
    QSpinBox* quantityInput_ = nullptr;
    QProgressBar* loadProgress_ = nullptr;
    QLabel* replayStatus_ = nullptr;
    QLabel* metricSummary_ = nullptr;
    QLabel* queueSummary_ = nullptr;
    QLabel* feedSummary_ = nullptr;
    QLabel* bookSummary_ = nullptr;
    QLabel* submittedMetricValue_ = nullptr;
    QLabel* acceptedMetricValue_ = nullptr;
    QLabel* tradeMetricValue_ = nullptr;
    QLabel* latencyMetricValue_ = nullptr;
    QLabel* percentileMetricValue_ = nullptr;
    QLabel* eventMetricValue_ = nullptr;
    TreeGraphicsView* graphicsView_ = nullptr;
    QGraphicsScene* graphicsScene_ = nullptr;
    QStackedWidget* pageStack_ = nullptr;
    QTabWidget* bookTabs_ = nullptr;
    QPushButton* bookNavButton_ = nullptr;
    QPushButton* ordersNavButton_ = nullptr;
    QPushButton* replayNavButton_ = nullptr;
    QPushButton* metricsNavButton_ = nullptr;
    QPushButton* csvNavButton_ = nullptr;
    QPushButton* submitOrderButton_ = nullptr;
    QPushButton* cancelOrderButton_ = nullptr;
    QPushButton* modifyOrderButton_ = nullptr;
    QPushButton* cancelBatchButton_ = nullptr;
    QPushButton* loadOrderButton_ = nullptr;
    QPushButton* resetBookButton_ = nullptr;
    QTimer* batchTimer_ = nullptr;
    QTimer* dashboardTimer_ = nullptr;
    QCustomPlot* latencyPlot_ = nullptr;
    QCustomPlot* activityPlot_ = nullptr;
    std::vector<double> latencyPoints_;
    std::size_t plottedActivitySubmission_ = 0;
};

#endif
