#ifndef GUI_MAIN_WINDOW_HPP
#define GUI_MAIN_WINDOW_HPP

#include <QMainWindow>

#include <chrono>
#include <cstdint>
#include <vector>

#include "CsvTableModel.hpp"
#include "CsvWorker.hpp"
#include "EngineWorker.hpp"

class QColor;
class QComboBox;
class QGraphicsView;
class QGraphicsScene;
class QLabel;
class QLineEdit;
class QCloseEvent;
class QProgressBar;
class QPushButton;
class QThread;
class QStackedWidget;
class QString;
class QSpinBox;
class QTableWidget;
class QTableView;
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

protected:
    void closeEvent(QCloseEvent* event) override;

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
    void resetReplay();
    void fastForwardReplay();
    void updateReplayProgress(std::size_t cursor);
    void chooseReplayFile();
    void loadReplay(const QString& path);
    void readReplayFile(const QString& path);
    void loadReplayEventPage(int page);
    void displayReplayEventPage(const EngineReply& reply);
    void validateReplayAgainstLive();
    void updateReplayView(const ReplaySnapshot& snapshot);
    void chooseCsvFile();
    void loadCsv(const QString& path);
    void recordMetric(double latencyMs);
    void updateMetricsDashboard();
    void exportMetrics();
    void openCurrentEventLog();
    void loadCsvPage();
    void updateCsvPage(const struct CsvPageReply& reply);
    void showError(const QString& message);

    EngineWorker* engineWorker_ = nullptr;
    QThread* engineThread_ = nullptr;
    CsvWorker* csvWorker_ = nullptr;
    QThread* csvThread_ = nullptr;
    CsvTableModel* csvModel_ = nullptr;
    BookSnapshot bookSnapshot_;
    std::vector<OrderCommand> pendingOrders_;
    std::size_t nextOrderIndex_ = 0;
    std::size_t replayCursor_ = 0;
    std::size_t replayEventCount_ = 0;
    std::size_t replayEventPageStart_ = 0;
    std::uint64_t nextTimestamp_ = 1;
    bool fullSystemMode_ = false;
    bool engineReady_ = false;
    bool engineRequestPending_ = false;
    bool batchInFlight_ = false;
    bool batchCancelRequested_ = false;
    bool reducedMotion_ = false;
    bool treeOpenGl_ = true;
    bool latencyRangeLocked_ = false;
    bool activityRangeLocked_ = false;
    bool throughputRangeLocked_ = false;
    std::uint64_t nextRequestID_ = 1;
    std::uint64_t resetRequestID_ = 0;
    std::uint64_t replayValidationRequestID_ = 0;
    std::uint64_t replayFlushRequestID_ = 0;
    std::uint64_t replayLoadRequestID_ = 0;
    std::uint64_t replayStepRequestID_ = 0;
    std::uint64_t replayResetRequestID_ = 0;
    std::uint64_t replayFastForwardRequestID_ = 0;
    std::uint64_t replayEventPageRequestID_ = 0;
    std::uint64_t orderParseRequestID_ = 0;
    std::uint64_t csvRequestID_ = 0;
    std::uint64_t nextCsvRequestID_ = 1;
    QString pendingReplayPath_;
    QString csvPath_;
    QString metricsReportPath_;
    QString csvFilter_;
    int csvPage_ = 0;
    std::size_t csvTotalRows_ = 0;
    std::chrono::steady_clock::time_point sessionStart_;
    std::chrono::steady_clock::time_point lastRateSampleTime_;
    std::size_t submissionCalls_ = 0;
    std::size_t acceptedOrders_ = 0;
    std::size_t displayedTradeCount_ = 0;
    std::size_t previousRateSubmissionCalls_ = 0;
    std::size_t previousRateAcceptedOrders_ = 0;
    std::size_t previousRateTradeCount_ = 0;
    std::size_t throughputSampleIndex_ = 0;

    QTreeWidget* buyTree_ = nullptr;
    QTreeWidget* sellTree_ = nullptr;
    QTableWidget* queueTable_ = nullptr;
    QTableWidget* ordersTable_ = nullptr;
    QTableWidget* tradesTable_ = nullptr;
    QTableWidget* replayTable_ = nullptr;
    QTableWidget* eventTable_ = nullptr;
    QTableView* csvTable_ = nullptr;
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
    QProgressBar* replayProgress_ = nullptr;
    QProgressBar* csvProgress_ = nullptr;
    QLabel* replayStatus_ = nullptr;
    QLabel* metricSummary_ = nullptr;
    QLabel* queueSummary_ = nullptr;
    QLabel* feedSummary_ = nullptr;
    QLabel* csvStatus_ = nullptr;
    QLabel* artifactPaths_ = nullptr;
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
    QPushButton* resetChartZoomButton_ = nullptr;
    QPushButton* csvPreviousButton_ = nullptr;
    QPushButton* csvNextButton_ = nullptr;
    QLineEdit* csvFilterInput_ = nullptr;
    QLabel* csvPageLabel_ = nullptr;
    QTimer* csvSearchTimer_ = nullptr;
    QPushButton* replayPlayButton_ = nullptr;
    QComboBox* replaySpeedCombo_ = nullptr;
    QPushButton* replayEventPreviousButton_ = nullptr;
    QPushButton* replayEventNextButton_ = nullptr;
    QLabel* replayEventPageLabel_ = nullptr;
    QTimer* batchTimer_ = nullptr;
    QTimer* dashboardTimer_ = nullptr;
    QTimer* replayTimer_ = nullptr;
    QCustomPlot* latencyPlot_ = nullptr;
    QCustomPlot* activityPlot_ = nullptr;
    QCustomPlot* throughputPlot_ = nullptr;
    std::vector<double> latencyPoints_;
    std::size_t plottedActivitySubmission_ = 0;
};

#endif
