#include "MainWindow.hpp"

#include <qcustomplot.h>

#include <QAbstractItemView>
#include <QAction>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QCloseEvent>
#include <QEasingCurve>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QFont>
#include <QGridLayout>
#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QKeySequence>
#include <QMessageBox>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPen>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QSettings>
#include <QThread>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QTreeWidget>
#include <QTextStream>
#include <QStringList>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QDir>

#include <algorithm>
#include <numeric>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

#include "CsvReader.hpp"
#include "CsvWorker.hpp"

class TreeGraphicsView : public QGraphicsView {
public:
    explicit TreeGraphicsView(QGraphicsScene* scene, QWidget* parent = nullptr)
        : QGraphicsView(scene, parent) {}

    void zoomBy(double factor) {
        const double next = std::clamp(zoom_ * factor, 0.25, 3.0);
        const double applied = next / zoom_;
        zoom_ = next;
        scale(applied, applied);
    }

    void resetZoom() {
        resetTransform();
        zoom_ = 1.0;
    }

    void fitScene(const QRectF& rect) {
        resetTransform();
        fitInView(rect, Qt::KeepAspectRatio);
        zoom_ = transform().m11();
    }

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (event->angleDelta().y() == 0) {
            QGraphicsView::wheelEvent(event);
            return;
        }
        zoomBy(event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15);
        event->accept();
    }

private:
    double zoom_ = 1.0;
};

namespace {

QColor sideColor(Side side) {
    return side == Side::Buy ? QColor("#f38ba8") : QColor("#a6e3a1");
}

QTableWidgetItem* tableItem(const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QFrame* metricCard(const QString& title, QLabel*& valueLabel, const QColor& accent) {
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("metricCard"));
    card->setMinimumHeight(98);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    auto* titleLabel = new QLabel(title);
    titleLabel->setObjectName(QStringLiteral("mutedText"));
    valueLabel = new QLabel(QStringLiteral("0"));
    valueLabel->setObjectName(QStringLiteral("metricValue"));
    valueLabel->setStyleSheet(QStringLiteral("color: %1;").arg(accent.name()));
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    layout->addStretch();
    return card;
}

void colorAxis(QCPAxis* axis) {
    axis->setBasePen(QPen(QColor("#585b70")));
    axis->setTickPen(QPen(QColor("#585b70")));
    axis->setSubTickPen(QPen(QColor("#45475a")));
    axis->setTickLabelColor(QColor("#cdd6f4"));
    axis->setLabelColor(QColor("#cdd6f4"));
    axis->grid()->setPen(QPen(QColor("#313244"), 1, Qt::DotLine));
    axis->grid()->setSubGridPen(QPen(QColor("#252638"), 1, Qt::DotLine));
}

QString sideName(Side side) {
    return side == Side::Buy ? QStringLiteral("BUY") : QStringLiteral("SELL");
}

QString eventTypeName(EventType type) {
    switch (type) {
    case EventType::NEW_ORDER: return QStringLiteral("NEW ORDER");
    case EventType::CANCEL_ORDER: return QStringLiteral("CANCEL");
    case EventType::MODIFY_ORDER: return QStringLiteral("MODIFY");
    case EventType::TRADE: return QStringLiteral("TRADE");
    }
    return QStringLiteral("UNKNOWN");
}

void configureTable(QTableWidget* table, const QStringList& headers) {
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
}

QString csvEscape(QString value) {
    value.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    if (value.contains(QLatin1Char(',')) || value.contains(QLatin1Char('"')) ||
        value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r'))) {
        value = QLatin1Char('"') + value + QLatin1Char('"');
    }
    return value;
}

}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      csvSearchTimer_(new QTimer(this)),
      batchTimer_(new QTimer(this)),
      dashboardTimer_(new QTimer(this)),
      replayTimer_(new QTimer(this)) {
    resize(1500, 950);
    setWindowTitle(QStringLiteral("Limit Order Book - Matching Engine"));
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
    reducedMotion_ = settings.value(QStringLiteral("ui/reducedMotion"), false).toBool();
    treeOpenGl_ = settings.value(QStringLiteral("ui/openGLTree"), true).toBool();
    sessionStart_ = std::chrono::steady_clock::now();
    lastRateSampleTime_ = sessionStart_;
    createNavigation();

    batchTimer_->setInterval(1);
    connect(batchTimer_, &QTimer::timeout, this, &MainWindow::processLoadedBatch);
    connect(replayTimer_, &QTimer::timeout, this, &MainWindow::appendReplayStep);
    dashboardTimer_->setInterval(1000);
    connect(dashboardTimer_, &QTimer::timeout, this, &MainWindow::updateMetricsDashboard);
    dashboardTimer_->start();
    connect(modeCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (!engineReady_ || engineRequestPending_ || batchInFlight_) return;
                const bool newMode = index == 1;
                if (newMode == fullSystemMode_) return;
                if (submissionCalls_ != 0 || batchTimer_->isActive()) {
                    const auto answer = QMessageBox::question(
                        this, QStringLiteral("Change run mode"),
                        QStringLiteral("Changing the mode resets the live book and event log. Continue?"));
                    if (answer != QMessageBox::Yes) {
                        modeCombo_->blockSignals(true);
                        modeCombo_->setCurrentIndex(fullSystemMode_ ? 1 : 0);
                        modeCombo_->blockSignals(false);
                        return;
                    }
                }
                fullSystemMode_ = newMode;
                resetSession();
            });

    engineThread_ = new QThread(this);
    engineWorker_ = new EngineWorker;
    engineWorker_->moveToThread(engineThread_);
    connect(engineThread_, &QThread::finished, engineWorker_, &QObject::deleteLater);
    connect(engineWorker_, &EngineWorker::ready, this,
            [this](bool success, const QString& error, const BookSnapshot& snapshot) {
                if (!success) {
                    showError(QStringLiteral("Could not initialize the engine: %1").arg(error));
                    return;
                }
                engineReady_ = true;
                modeCombo_->setEnabled(true);
                setOrderActionsEnabled(true);
                sessionStart_ = std::chrono::steady_clock::now();
                lastRateSampleTime_ = sessionStart_;
                applyBookSnapshot(snapshot);
                statusBar()->showMessage(QStringLiteral("Ready - engine-only mode"));
            });
    connect(engineWorker_, &EngineWorker::operationCompleted,
            this, &MainWindow::handleEngineReply);
    engineThread_->start();
    modeCombo_->setEnabled(false);
    engineWorker_->initialize(fullSystemMode_);

    csvThread_ = new QThread(this);
    csvWorker_ = new CsvWorker;
    csvWorker_->moveToThread(csvThread_);
    connect(csvThread_, &QThread::finished, csvWorker_, &QObject::deleteLater);
    connect(csvWorker_, &CsvWorker::pageReady, this,
            [this](const CsvPageReply& reply) { updateCsvPage(reply); });
    connect(csvWorker_, &CsvWorker::progressChanged, this,
            [this](std::uint64_t requestID, int percent) {
                if (requestID == csvRequestID_) csvProgress_->setValue(percent);
            });
    csvThread_->start();
    csvSearchTimer_->setSingleShot(true);
    csvSearchTimer_->setInterval(350);
    connect(csvSearchTimer_, &QTimer::timeout, this, [this] {
        csvFilter_ = csvFilterInput_->text();
        csvPage_ = 0;
        loadCsvPage();
    });
    connect(csvFilterInput_, &QLineEdit::textChanged, this, [this] {
        if (!csvPath_.isEmpty()) {
            csvStatus_->setText(QStringLiteral("Searching file..."));
            csvSearchTimer_->start();
        }
    });
}

MainWindow::~MainWindow() {
    if (batchTimer_) batchTimer_->stop();
    if (dashboardTimer_) dashboardTimer_->stop();
    if (csvSearchTimer_) csvSearchTimer_->stop();
    if (csvWorker_) csvWorker_->cancelPending();
    if (csvThread_ && csvThread_->isRunning()) {
        csvThread_->quit();
        csvThread_->wait();
    }
    if (engineThread_ && engineThread_->isRunning()) {
        engineThread_->quit();
        engineThread_->wait();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.setValue(QStringLiteral("ui/reducedMotion"), reducedMotion_);
    settings.setValue(QStringLiteral("ui/openGLTree"), treeOpenGl_);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        statusBar()->showMessage(QStringLiteral("Could not save GUI preferences."), 5000);
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::createNavigation() {
    auto* central = new QWidget;
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* navigation = new QWidget;
    navigation->setObjectName(QStringLiteral("navigationBar"));
    auto* navigationLayout = new QHBoxLayout(navigation);
    navigationLayout->setContentsMargins(18, 10, 18, 10);
    navigationLayout->setSpacing(8);
    auto* brand = new QLabel(QStringLiteral("LIMIT ORDER BOOK"));
    brand->setObjectName(QStringLiteral("brandLabel"));
    navigationLayout->addWidget(brand);
    navigationLayout->addSpacing(22);

    auto makeButton = [navigation](const QString& text) {
        auto* button = new QPushButton(text, navigation);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCheckable(true);
        button->setMinimumHeight(36);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };
    bookNavButton_ = makeButton(QStringLiteral("Order Book"));
    ordersNavButton_ = makeButton(QStringLiteral("Orders"));
    replayNavButton_ = makeButton(QStringLiteral("Replay"));
    metricsNavButton_ = makeButton(QStringLiteral("Metrics"));
    csvNavButton_ = makeButton(QStringLiteral("CSV Viewer"));
    bookNavButton_->setAccessibleName(QStringLiteral("Show Order Book page"));
    ordersNavButton_->setAccessibleName(QStringLiteral("Show Orders page"));
    replayNavButton_->setAccessibleName(QStringLiteral("Show Replay page"));
    metricsNavButton_->setAccessibleName(QStringLiteral("Show Metrics page"));
    csvNavButton_->setAccessibleName(QStringLiteral("Show CSV Viewer page"));
    for (QPushButton* button : {bookNavButton_, ordersNavButton_, replayNavButton_,
                                metricsNavButton_, csvNavButton_}) {
        navigationLayout->addWidget(button);
    }
    navigationLayout->addStretch();
    layout->addWidget(navigation);

    pageStack_ = new QStackedWidget(central);
    pageStack_->addWidget(createBookPanel());
    pageStack_->addWidget(createOrderPanel());
    pageStack_->addWidget(createReplayPanel());
    pageStack_->addWidget(createMetricsPanel());
    pageStack_->addWidget(createCsvPanel());
    layout->addWidget(pageStack_, 1);
    setCentralWidget(central);

    const QList<QPushButton*> buttons = {
        bookNavButton_, ordersNavButton_, replayNavButton_, metricsNavButton_, csvNavButton_};
    for (int page = 0; page < buttons.size(); ++page) {
        connect(buttons[page], &QPushButton::clicked, this, [this, page, buttons] {
            pageStack_->setCurrentIndex(page);
            for (int index = 0; index < buttons.size(); ++index) {
                buttons[index]->setChecked(index == page);
            }
            for (int page = 0; page < buttons.size(); ++page) {
                auto* action = new QAction(this);
                action->setShortcut(QKeySequence(QStringLiteral("Ctrl+%1").arg(page + 1)));
                action->setShortcutContext(Qt::ApplicationShortcut);
                connect(action, &QAction::triggered, this, [buttons, page] {
                    buttons[page]->click();
                });
                addAction(action);
            }
        });
    }
    bookNavButton_->setChecked(true);
}

QWidget* MainWindow::createBookPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);

    auto* controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("Runtime path:")));
    modeCombo_ = new QComboBox;
    modeCombo_->setMaximumWidth(420);
    modeCombo_->addItems({QStringLiteral("Engine only (Worklets 1-2)"),
                          QStringLiteral("Full system (Worklets 1-3)")});
    controls->addWidget(modeCombo_);
    auto* openGlToggle = new QCheckBox(QStringLiteral("OpenGL tree view"));
    openGlToggle->setAccessibleDescription(
        QStringLiteral("Use the OpenGL viewport for tree rendering; disable if unsupported."));
    openGlToggle->setChecked(treeOpenGl_);
    connect(openGlToggle, &QCheckBox::toggled, this, [this](bool enabled) {
        treeOpenGl_ = enabled;
        graphicsView_->setViewport(enabled ? static_cast<QWidget*>(new QOpenGLWidget)
                                           : new QWidget);
    });
    controls->addWidget(openGlToggle);
    auto* reducedMotionToggle = new QCheckBox(QStringLiteral("Reduce animations"));
    reducedMotionToggle->setAccessibleName(QStringLiteral("Reduce trade animations"));
    reducedMotionToggle->setChecked(reducedMotion_);
    connect(reducedMotionToggle, &QCheckBox::toggled, this,
            [this](bool enabled) { reducedMotion_ = enabled; });
    controls->addWidget(reducedMotionToggle);
    auto* zoomOut = new QPushButton(QStringLiteral("-"));
    zoomOut->setToolTip(QStringLiteral("Zoom out (mouse wheel down over the tree also works)"));
    zoomOut->setMaximumWidth(38);
    connect(zoomOut, &QPushButton::clicked, this, [this] { zoomBookView(1.0 / 1.2); });
    controls->addWidget(zoomOut);
    auto* zoomIn = new QPushButton(QStringLiteral("+"));
    zoomIn->setToolTip(QStringLiteral("Zoom in (mouse wheel up over the tree also works)"));
    zoomIn->setMaximumWidth(38);
    connect(zoomIn, &QPushButton::clicked, this, [this] { zoomBookView(1.2); });
    controls->addWidget(zoomIn);
    auto* fitTree = new QPushButton(QStringLiteral("Fit tree"));
    fitTree->setToolTip(QStringLiteral("Fit both AVL trees in the viewer"));
    connect(fitTree, &QPushButton::clicked, this, &MainWindow::fitBookView);
    controls->addWidget(fitTree);
    controls->addStretch(1);
    resetBookButton_ = new QPushButton(QStringLiteral("Reset book"));
    connect(resetBookButton_, &QPushButton::clicked, this, [this] {
        if (batchInFlight_ || batchTimer_->isActive() || engineRequestPending_) return;
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Reset book"),
            QStringLiteral("Clear all live orders, trades, and the current event log?"));
        if (answer == QMessageBox::Yes) resetSession();
    });
    resetBookButton_->setMaximumWidth(180);
    controls->addWidget(resetBookButton_);
    layout->addLayout(controls);
    bookSummary_ = new QLabel(QStringLiteral("Best bid: -   Best ask: -   Spread: -"));
    bookSummary_->setObjectName(QStringLiteral("feedSummary"));
    layout->addWidget(bookSummary_);

    graphicsScene_ = new QGraphicsScene(this);
    graphicsScene_->setBackgroundBrush(QColor("#181825"));
    graphicsView_ = new TreeGraphicsView(graphicsScene_);
    graphicsView_->setMinimumHeight(320);
    graphicsView_->setRenderHint(QPainter::Antialiasing);
    graphicsView_->setViewport(new QOpenGLWidget);
    graphicsView_->setBackgroundBrush(QColor("#181825"));
    graphicsView_->setDragMode(QGraphicsView::ScrollHandDrag);
    graphicsView_->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    graphicsView_->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    graphicsView_->setMinimumSize(500, 300);
    graphicsView_->setAccessibleName(QStringLiteral("Buy and sell AVL tree visualization"));
    graphicsView_->setAccessibleDescription(
        QStringLiteral("Select a price-level node to view the FIFO order queue."));
    layout->addWidget(graphicsView_, 3);

    connect(graphicsScene_, &QGraphicsScene::selectionChanged, this, [this] {
        if (graphicsScene_->selectedItems().empty()) return;
        const QGraphicsItem* selected = graphicsScene_->selectedItems().front();
        const Side side = selected->data(0).toInt() == 0 ? Side::Buy : Side::Sell;
        refreshQueue(side, selected->data(1).toInt());
        bookTabs_->setCurrentWidget(queueTable_->parentWidget());
    });

    auto* treeSplit = new QSplitter(Qt::Horizontal);
    buyTree_ = new QTreeWidget;
    sellTree_ = new QTreeWidget;
    buyTree_->setHeaderLabels({QStringLiteral("BUY AVL"), QStringLiteral("Qty"), QStringLiteral("Orders")});
    sellTree_->setHeaderLabels({QStringLiteral("SELL AVL"), QStringLiteral("Qty"), QStringLiteral("Orders")});
    buyTree_->setAccessibleName(QStringLiteral("Buy price-level tree"));
    sellTree_->setAccessibleName(QStringLiteral("Sell price-level tree"));
    buyTree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    sellTree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    treeSplit->addWidget(buyTree_);
    treeSplit->addWidget(sellTree_);
    bookTabs_ = new QTabWidget;
    bookTabs_->addTab(treeSplit, QStringLiteral("Price Levels"));
    bookTabs_->addTab(createQueuePanel(), QStringLiteral("FIFO Queue"));
    ordersTable_ = new QTableWidget;
    configureTable(ordersTable_, {QStringLiteral("ID"), QStringLiteral("Side"),
        QStringLiteral("Price"), QStringLiteral("Remaining"), QStringLiteral("FIFO")});
    bookTabs_->addTab(ordersTable_, QStringLiteral("Open Orders"));
    tradesTable_ = new QTableWidget;
    configureTable(tradesTable_, {QStringLiteral("Trade"), QStringLiteral("Buy ID"),
        QStringLiteral("Sell ID"), QStringLiteral("Price"), QStringLiteral("Quantity")});
    bookTabs_->addTab(tradesTable_, QStringLiteral("Recent Trades"));
    layout->addWidget(bookTabs_, 2);

    connect(buyTree_, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* item, int) { refreshQueue(buyTree_, item); });
    connect(sellTree_, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* item, int) { refreshQueue(sellTree_, item); });

    return panel;
}

void MainWindow::zoomBookView(double factor) {
    if (graphicsView_) graphicsView_->zoomBy(factor);
}

void MainWindow::fitBookView() {
    if (!graphicsView_ || !graphicsScene_) return;
    graphicsView_->fitScene(graphicsScene_->sceneRect());
}

QWidget* MainWindow::createOrderPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    auto* splitter = new QSplitter(Qt::Horizontal, panel);

    auto* formPanel = new QFrame;
    formPanel->setObjectName(QStringLiteral("panelCard"));
    formPanel->setMinimumWidth(350);
    auto* formLayout = new QVBoxLayout(formPanel);
    auto* title = new QLabel(QStringLiteral("Order controls"));
    title->setObjectName(QStringLiteral("sectionTitle"));
    formLayout->addWidget(title);
    auto* subtitle = new QLabel(QStringLiteral(
        "Submit new orders or select an open order on the right to cancel or modify it."));
    subtitle->setWordWrap(true);
    subtitle->setObjectName(QStringLiteral("mutedText"));
    formLayout->addWidget(subtitle);
    auto* form = new QGridLayout;
    form->addWidget(new QLabel(QStringLiteral("Order ID")), 0, 0);
    idInput_ = new QSpinBox;
    idInput_->setAccessibleName(QStringLiteral("Order ID"));
    idInput_->setRange(1, std::numeric_limits<int>::max());
    form->addWidget(idInput_, 0, 1);
    form->addWidget(new QLabel(QStringLiteral("Type")), 1, 0);
    typeCombo_ = new QComboBox;
    typeCombo_->setAccessibleName(QStringLiteral("Order type"));
    typeCombo_->addItems({QStringLiteral("LIMIT"), QStringLiteral("MARKET")});
    form->addWidget(typeCombo_, 1, 1);
    form->addWidget(new QLabel(QStringLiteral("Side")), 2, 0);
    sideCombo_ = new QComboBox;
    sideCombo_->setAccessibleName(QStringLiteral("Order side"));
    sideCombo_->setObjectName(QStringLiteral("sideCombo"));
    sideCombo_->addItems({QStringLiteral("BUY"), QStringLiteral("SELL")});
    sideCombo_->setItemData(0, sideColor(Side::Buy), Qt::ForegroundRole);
    sideCombo_->setItemData(1, sideColor(Side::Sell), Qt::ForegroundRole);
    form->addWidget(sideCombo_, 2, 1);
    form->addWidget(new QLabel(QStringLiteral("Price")), 3, 0);
    priceInput_ = new QSpinBox;
    priceInput_->setAccessibleName(QStringLiteral("Order price"));
    priceInput_->setRange(0, std::numeric_limits<int>::max());
    form->addWidget(priceInput_, 3, 1);
    form->addWidget(new QLabel(QStringLiteral("Quantity")), 4, 0);
    quantityInput_ = new QSpinBox;
    quantityInput_->setAccessibleName(QStringLiteral("Order quantity"));
    quantityInput_->setRange(1, std::numeric_limits<int>::max());
    form->addWidget(quantityInput_, 4, 1);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(12);
    formLayout->addLayout(form);

    connect(typeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                const bool market = index == 1;
                priceInput_->setEnabled(!market);
                if (market) priceInput_->setValue(0);
            });
    connect(sideCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                sideCombo_->setStyleSheet(QStringLiteral("QComboBox#sideCombo { color: %1; }")
                    .arg(sideColor(index == 0 ? Side::Buy : Side::Sell).name()));
            });
    sideCombo_->setStyleSheet(QStringLiteral("QComboBox#sideCombo { color: %1; }")
        .arg(sideColor(Side::Buy).name()));

    submitOrderButton_ = new QPushButton(QStringLiteral("Submit order"));
    connect(submitOrderButton_, &QPushButton::clicked, this, &MainWindow::submitOrder);
    submitOrderButton_->setObjectName(QStringLiteral("primaryButton"));
    formLayout->addWidget(submitOrderButton_);
    cancelOrderButton_ = new QPushButton(QStringLiteral("Cancel by ID"));
    connect(cancelOrderButton_, &QPushButton::clicked, this, &MainWindow::cancelOrder);
    formLayout->addWidget(cancelOrderButton_);
    modifyOrderButton_ = new QPushButton(QStringLiteral("Modify ID / price / quantity"));
    connect(modifyOrderButton_, &QPushButton::clicked, this, &MainWindow::modifyOrder);
    formLayout->addWidget(modifyOrderButton_);

    loadOrderButton_ = new QPushButton(QStringLiteral("Load JSON / CSV orders"));
    connect(loadOrderButton_, &QPushButton::clicked, this, &MainWindow::chooseOrderFile);
    formLayout->addSpacing(14);
    formLayout->addWidget(loadOrderButton_);
    loadProgress_ = new QProgressBar;
    loadProgress_->setRange(0, 100);
    loadProgress_->setValue(0);
    formLayout->addWidget(loadProgress_);
    cancelBatchButton_ = new QPushButton(QStringLiteral("Cancel order batch"));
    cancelBatchButton_->setEnabled(false);
    connect(cancelBatchButton_, &QPushButton::clicked, this, [this] {
        batchCancelRequested_ = true;
        batchTimer_->stop();
        if (!batchInFlight_) {
            pendingOrders_.clear();
            nextOrderIndex_ = 0;
            loadProgress_->setValue(0);
            setOrderActionsEnabled(true);
            cancelBatchButton_->setEnabled(false);
            statusBar()->showMessage(QStringLiteral("Order batch canceled."), 4000);
        } else {
            statusBar()->showMessage(
                QStringLiteral("Stopping after the current batch finishes..."), 4000);
        }
    });
    formLayout->addWidget(cancelBatchButton_);
    formLayout->addStretch();
    splitter->addWidget(formPanel);

    auto* activityPanel = new QWidget;
    auto* activityLayout = new QVBoxLayout(activityPanel);
    activityLayout->setContentsMargins(14, 0, 0, 0);
    auto* activityTitle = new QLabel(QStringLiteral("Live order activity"));
    activityTitle->setObjectName(QStringLiteral("sectionTitle"));
    activityLayout->addWidget(activityTitle);
    auto* activityTabs = new QTabWidget;
    orderPageOrdersTable_ = new QTableWidget;
    configureTable(orderPageOrdersTable_, {QStringLiteral("Order ID"), QStringLiteral("Side"),
        QStringLiteral("Price"), QStringLiteral("Remaining"), QStringLiteral("FIFO")});
    orderPageOrdersTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    activityTabs->addTab(orderPageOrdersTable_, QStringLiteral("Open orders"));
    orderPageTradesTable_ = new QTableWidget;
    configureTable(orderPageTradesTable_, {QStringLiteral("Trade"), QStringLiteral("Buy ID"),
        QStringLiteral("Sell ID"), QStringLiteral("Price"), QStringLiteral("Quantity")});
    activityTabs->addTab(orderPageTradesTable_, QStringLiteral("Recent executions"));
    orderHistoryTable_ = new QTableWidget;
    configureTable(orderHistoryTable_, {QStringLiteral("Order ID"), QStringLiteral("Side"),
        QStringLiteral("Type"), QStringLiteral("Requested"), QStringLiteral("Executed"),
        QStringLiteral("Remaining"), QStringLiteral("Outcome")});
    activityTabs->addTab(orderHistoryTable_, QStringLiteral("Order history"));
    activityLayout->addWidget(activityTabs);
    splitter->addWidget(activityPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 5);
    splitter->setSizes({430, 1050});
    layout->addWidget(splitter);

    connect(orderPageOrdersTable_, &QTableWidget::itemSelectionChanged, this, [this] {
        const int row = orderPageOrdersTable_->currentRow();
        if (row < 0 || !orderPageOrdersTable_->item(row, 0)) return;
        idInput_->setValue(orderPageOrdersTable_->item(row, 0)->text().toInt());
        sideCombo_->setCurrentIndex(orderPageOrdersTable_->item(row, 1)->text() == "BUY" ? 0 : 1);
        priceInput_->setValue(orderPageOrdersTable_->item(row, 2)->text().toInt());
        quantityInput_->setValue(orderPageOrdersTable_->item(row, 3)->text().toInt());
    });
    return panel;
}

QWidget* MainWindow::createQueuePanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    queueSummary_ = new QLabel(QStringLiteral("Select a rendered AVL node or price level to inspect its FIFO queue."));
    layout->addWidget(queueSummary_);
    queueTable_ = new QTableWidget;
    configureTable(queueTable_, {QStringLiteral("FIFO"), QStringLiteral("Order ID"),
        QStringLiteral("Remaining"), QStringLiteral("Timestamp")});
    layout->addWidget(queueTable_);
    return panel;
}

QWidget* MainWindow::createReplayPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(12);
    auto* heading = new QLabel(QStringLiteral("Worklet 3 - Event Replay & Reconstruction"));
    heading->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(heading);
    auto* controls = new QHBoxLayout;
    auto* load = new QPushButton(QStringLiteral("Open event log"));
    connect(load, &QPushButton::clicked, this, &MainWindow::chooseReplayFile);
    controls->addWidget(load);
    auto* next = new QPushButton(QStringLiteral("Step forward"));
    connect(next, &QPushButton::clicked, this, &MainWindow::appendReplayStep);
    controls->addWidget(next);
    replayPlayButton_ = new QPushButton(QStringLiteral("Play"));
    connect(replayPlayButton_, &QPushButton::clicked, this, [this] {
        if (replayTimer_->isActive()) {
            replayTimer_->stop();
            replayPlayButton_->setText(QStringLiteral("Play"));
        } else if (replayEventCount_ != 0) {
            replayPlayButton_->setText(QStringLiteral("Pause"));
            replayTimer_->start(replaySpeedCombo_->currentData().toInt());
        }
    });
    controls->addWidget(replayPlayButton_);
    replaySpeedCombo_ = new QComboBox;
    replaySpeedCombo_->setAccessibleName(QStringLiteral("Replay speed"));
    replaySpeedCombo_->addItem(QStringLiteral("0.5 orders/s"), 2000);
    replaySpeedCombo_->addItem(QStringLiteral("1 order/s"), 1000);
    replaySpeedCombo_->addItem(QStringLiteral("2 orders/s"), 500);
    replaySpeedCombo_->addItem(QStringLiteral("5 orders/s"), 200);
    replaySpeedCombo_->addItem(QStringLiteral("10 orders/s"), 100);
    replaySpeedCombo_->setCurrentIndex(1);
    connect(replaySpeedCombo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this] {
                if (replayTimer_->isActive()) {
                    replayTimer_->start(replaySpeedCombo_->currentData().toInt());
                }
            });
    controls->addWidget(replaySpeedCombo_);
    auto* fastForward = new QPushButton(QStringLiteral("Fast-forward"));
    connect(fastForward, &QPushButton::clicked, this, &MainWindow::fastForwardReplay);
    controls->addWidget(fastForward);
    auto* resetReplayButton = new QPushButton(QStringLiteral("Reset replay"));
    connect(resetReplayButton, &QPushButton::clicked, this, &MainWindow::resetReplay);
    controls->addWidget(resetReplayButton);
    auto* validate = new QPushButton(QStringLiteral("Validate against live book"));
    validate->setObjectName(QStringLiteral("primaryButton"));
    connect(validate, &QPushButton::clicked, this, &MainWindow::validateReplayAgainstLive);
    controls->addWidget(validate);
    controls->addStretch();
    layout->addLayout(controls);
    replayStatus_ = new QLabel(QStringLiteral("No event log loaded."));
    replayStatus_->setWordWrap(true);
    replayStatus_->setObjectName(QStringLiteral("statusBanner"));
    layout->addWidget(replayStatus_);
    replayProgress_ = new QProgressBar;
    replayProgress_->setRange(0, 1000);
    replayProgress_->setValue(0);
    replayProgress_->setAccessibleName(QStringLiteral("Replay progress"));
    layout->addWidget(replayProgress_);

    auto* tabs = new QTabWidget;
    replayTable_ = new QTableWidget;
    configureTable(replayTable_, {QStringLiteral("Side"), QStringLiteral("Price"),
        QStringLiteral("Total quantity"), QStringLiteral("Order IDs (FIFO)")});
    tabs->addTab(replayTable_, QStringLiteral("Reconstructed book"));
    auto* eventPage = new QWidget;
    auto* eventPageLayout = new QVBoxLayout(eventPage);
    auto* eventPageControls = new QHBoxLayout;
    replayEventPreviousButton_ = new QPushButton(QStringLiteral("Previous 500 events"));
    replayEventPreviousButton_->setEnabled(false);
    connect(replayEventPreviousButton_, &QPushButton::clicked, this, [this] {
        if (replayEventPageStart_ >= 500) {
            loadReplayEventPage(static_cast<int>(replayEventPageStart_ / 500) - 1);
        }
    });
    eventPageControls->addWidget(replayEventPreviousButton_);
    replayEventPageLabel_ = new QLabel(QStringLiteral("No events loaded"));
    eventPageControls->addWidget(replayEventPageLabel_, 1);
    replayEventNextButton_ = new QPushButton(QStringLiteral("Next 500 events"));
    replayEventNextButton_->setEnabled(false);
    connect(replayEventNextButton_, &QPushButton::clicked, this, [this] {
        loadReplayEventPage(static_cast<int>(replayEventPageStart_ / 500) + 1);
    });
    eventPageControls->addWidget(replayEventNextButton_);
    eventPageLayout->addLayout(eventPageControls);
    eventTable_ = new QTableWidget;
    configureTable(eventTable_, {QStringLiteral("Sequence"), QStringLiteral("Event"),
        QStringLiteral("Side"), QStringLiteral("Order ID"), QStringLiteral("Trade IDs"),
        QStringLiteral("Price"), QStringLiteral("Quantity")});
    eventTable_->setAccessibleName(QStringLiteral("Paged event log"));
    eventPageLayout->addWidget(eventTable_);
    tabs->addTab(eventPage, QStringLiteral("Event log"));
    layout->addWidget(tabs, 1);
    return panel;
}

QWidget* MainWindow::createCsvPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    auto* controls = new QHBoxLayout;
    auto* load = new QPushButton(QStringLiteral("Open CSV file"));
    connect(load, &QPushButton::clicked, this, &MainWindow::chooseCsvFile);
    controls->addWidget(load);
    csvFilterInput_ = new QLineEdit;
    csvFilterInput_->setPlaceholderText(QStringLiteral("Search all CSV rows..."));
    csvFilterInput_->setAccessibleName(QStringLiteral("Search CSV file"));
    controls->addWidget(csvFilterInput_, 1);
    csvPreviousButton_ = new QPushButton(QStringLiteral("Previous"));
    csvPreviousButton_->setEnabled(false);
    connect(csvPreviousButton_, &QPushButton::clicked, this, [this] {
        if (csvPage_ > 0) {
            --csvPage_;
            loadCsvPage();
        }
    });
    controls->addWidget(csvPreviousButton_);
    csvPageLabel_ = new QLabel(QStringLiteral("No CSV loaded"));
    controls->addWidget(csvPageLabel_);
    csvNextButton_ = new QPushButton(QStringLiteral("Next"));
    csvNextButton_->setEnabled(false);
    connect(csvNextButton_, &QPushButton::clicked, this, [this] {
        const std::size_t nextStart = static_cast<std::size_t>(csvPage_ + 1) * 500;
        if (nextStart < csvTotalRows_) {
            ++csvPage_;
            loadCsvPage();
        }
    });
    controls->addWidget(csvNextButton_);
    layout->addLayout(controls);
    csvStatus_ = new QLabel(QStringLiteral(
        "Search scans the whole file; clicking a column sorts the displayed page."));
    csvStatus_->setObjectName(QStringLiteral("mutedText"));
    layout->addWidget(csvStatus_);
    csvProgress_ = new QProgressBar;
    csvProgress_->setRange(0, 100);
    csvProgress_->setValue(0);
    layout->addWidget(csvProgress_);
    csvTable_ = new QTableView;
    csvModel_ = new CsvTableModel(this);
    auto* sortModel = new QSortFilterProxyModel(csvTable_);
    sortModel->setSourceModel(csvModel_);
    sortModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    csvTable_->setModel(sortModel);
    csvTable_->setSortingEnabled(true);
    csvTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    csvTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    csvTable_->setAlternatingRowColors(true);
    csvTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    csvTable_->horizontalHeader()->setStretchLastSection(true);
    csvTable_->setAccessibleName(QStringLiteral("CSV data table"));
    layout->addWidget(csvTable_, 1);
    return panel;
}

QWidget* MainWindow::createMetricsPanel() {
    auto* panel = new QWidget;
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(14);
    auto* heading = new QLabel(QStringLiteral("Performance & Worklet 3"));
    heading->setObjectName(QStringLiteral("pageTitle"));
    layout->addWidget(heading);
    metricSummary_ = new QLabel(QStringLiteral(
        "Latency is measured around submitOrderDetailed. Session rates include time spent idle; use the CLI benchmark for capacity measurements."));
    metricSummary_->setWordWrap(true);
    layout->addWidget(metricSummary_);

    auto* cards = new QHBoxLayout;
    cards->setSpacing(12);
    cards->addWidget(metricCard(QStringLiteral("SUBMISSION CALLS"),
                                submittedMetricValue_, QColor("#89b4fa")));
    cards->addWidget(metricCard(QStringLiteral("ACCEPTED ORDERS"),
                                acceptedMetricValue_, QColor("#a6e3a1")));
    cards->addWidget(metricCard(QStringLiteral("TRADE EXECUTIONS"),
                                tradeMetricValue_, QColor("#f9e2af")));
    cards->addWidget(metricCard(QStringLiteral("MEAN LATENCY"),
                                latencyMetricValue_, QColor("#cba6f7")));
    cards->addWidget(metricCard(QStringLiteral("P95 / P99 LATENCY"),
                                percentileMetricValue_, QColor("#fab387")));
    cards->addWidget(metricCard(QStringLiteral("EVENTS LOGGED"),
                                eventMetricValue_, QColor("#94e2d5")));
    layout->addLayout(cards);

    feedSummary_ = new QLabel;
    feedSummary_->setObjectName(QStringLiteral("feedSummary"));
    feedSummary_->setWordWrap(true);
    layout->addWidget(feedSummary_);
    auto* artifactControls = new QHBoxLayout;
    auto* exportMetricsButton = new QPushButton(QStringLiteral("Export run metrics CSV"));
    exportMetricsButton->setAccessibleName(QStringLiteral("Export run metrics CSV"));
    connect(exportMetricsButton, &QPushButton::clicked, this, &MainWindow::exportMetrics);
    artifactControls->addWidget(exportMetricsButton);
    auto* openEventLogButton = new QPushButton(QStringLiteral("Open current event log"));
    connect(openEventLogButton, &QPushButton::clicked, this, &MainWindow::openCurrentEventLog);
    artifactControls->addWidget(openEventLogButton);
    auto* viewReportButton = new QPushButton(QStringLiteral("View exported CSV"));
    connect(viewReportButton, &QPushButton::clicked, this, [this] {
        if (metricsReportPath_.isEmpty()) {
            showError(QStringLiteral("Export a run metrics CSV first."));
            return;
        }
        csvNavButton_->click();
        loadCsv(metricsReportPath_);
    });
    artifactControls->addWidget(viewReportButton);
    artifactControls->addStretch();
    resetChartZoomButton_ = new QPushButton(QStringLiteral("Reset chart zoom"));
    resetChartZoomButton_->setToolTip(
        QStringLiteral("Resume automatic tracking for all metric charts."));
    connect(resetChartZoomButton_, &QPushButton::clicked, this, [this] {
        latencyRangeLocked_ = false;
        activityRangeLocked_ = false;
        throughputRangeLocked_ = false;
        updateMetricsDashboard();
    });
    artifactControls->addWidget(resetChartZoomButton_);
    layout->addLayout(artifactControls);
    artifactPaths_ = new QLabel;
    artifactPaths_->setObjectName(QStringLiteral("mutedText"));
    artifactPaths_->setWordWrap(true);
    layout->addWidget(artifactPaths_);

    auto* chartTabs = new QTabWidget;
    latencyPlot_ = new QCustomPlot;
    latencyPlot_->setObjectName(QStringLiteral("latencyPlot"));
    latencyPlot_->setBackground(QColor("#181825"));
    latencyPlot_->addGraph();
    latencyPlot_->graph(0)->setPen(QPen(QColor("#cba6f7"), 2));
    latencyPlot_->graph(0)->setName(QStringLiteral("Submission latency"));
    latencyPlot_->legend->setVisible(true);
    latencyPlot_->legend->setBrush(QColor("#1e1e2e"));
    latencyPlot_->legend->setTextColor(QColor("#cdd6f4"));
    colorAxis(latencyPlot_->xAxis);
    colorAxis(latencyPlot_->yAxis);
    latencyPlot_->xAxis->setLabel(QStringLiteral("Submission number"));
    latencyPlot_->yAxis->setLabel(QStringLiteral("Submission latency (ms)"));
    latencyPlot_->xAxis->setRange(0, 10);
    latencyPlot_->yAxis->setRange(0, 0.01);
    latencyPlot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    latencyPlot_->setMinimumHeight(420);
    chartTabs->addTab(latencyPlot_, QStringLiteral("Latency"));

    activityPlot_ = new QCustomPlot;
    activityPlot_->setObjectName(QStringLiteral("activityPlot"));
    activityPlot_->setBackground(QColor("#181825"));
    activityPlot_->legend->setVisible(true);
    activityPlot_->legend->setBrush(QColor("#1e1e2e"));
    activityPlot_->legend->setTextColor(QColor("#cdd6f4"));
    for (int i = 0; i < 3; ++i) activityPlot_->addGraph();
    activityPlot_->graph(0)->setName(QStringLiteral("Accepted orders"));
    activityPlot_->graph(0)->setPen(QPen(QColor("#a6e3a1"), 2));
    activityPlot_->graph(1)->setName(QStringLiteral("Trades"));
    activityPlot_->graph(1)->setPen(QPen(QColor("#f9e2af"), 2));
    activityPlot_->graph(2)->setName(QStringLiteral("Worklet 3 events"));
    activityPlot_->graph(2)->setPen(QPen(QColor("#89b4fa"), 2));
    colorAxis(activityPlot_->xAxis);
    colorAxis(activityPlot_->yAxis);
    activityPlot_->xAxis->setLabel(QStringLiteral("Submission number"));
    activityPlot_->yAxis->setLabel(QStringLiteral("Cumulative count"));
    activityPlot_->xAxis->setRange(0, 10);
    activityPlot_->yAxis->setRange(0, 10);
    activityPlot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    activityPlot_->setMinimumHeight(420);
    chartTabs->addTab(activityPlot_, QStringLiteral("Session activity"));

    throughputPlot_ = new QCustomPlot;
    throughputPlot_->setObjectName(QStringLiteral("throughputPlot"));
    throughputPlot_->setBackground(QColor("#181825"));
    throughputPlot_->legend->setVisible(true);
    throughputPlot_->legend->setBrush(QColor("#1e1e2e"));
    throughputPlot_->legend->setTextColor(QColor("#cdd6f4"));
    for (int graph = 0; graph < 3; ++graph) throughputPlot_->addGraph();
    throughputPlot_->graph(0)->setName(QStringLiteral("Submitted orders/s (TPS)"));
    throughputPlot_->graph(0)->setPen(QPen(QColor("#89b4fa"), 2));
    throughputPlot_->graph(1)->setName(QStringLiteral("Accepted orders/s"));
    throughputPlot_->graph(1)->setPen(QPen(QColor("#a6e3a1"), 2));
    throughputPlot_->graph(2)->setName(QStringLiteral("Trade executions/s"));
    throughputPlot_->graph(2)->setPen(QPen(QColor("#f9e2af"), 2));
    colorAxis(throughputPlot_->xAxis);
    colorAxis(throughputPlot_->yAxis);
    throughputPlot_->xAxis->setLabel(QStringLiteral("Rate sample"));
    throughputPlot_->yAxis->setLabel(QStringLiteral("Rate (operations/s)"));
    throughputPlot_->xAxis->setRange(0, 60);
    throughputPlot_->yAxis->setRange(0, 10);
    throughputPlot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    throughputPlot_->setMinimumHeight(420);
    chartTabs->addTab(throughputPlot_, QStringLiteral("Throughput"));
    connect(latencyPlot_, &QCustomPlot::mousePress, this,
            [this](QMouseEvent*) { latencyRangeLocked_ = true; });
    connect(latencyPlot_, &QCustomPlot::mouseWheel, this,
            [this](QWheelEvent*) { latencyRangeLocked_ = true; });
    connect(activityPlot_, &QCustomPlot::mousePress, this,
            [this](QMouseEvent*) { activityRangeLocked_ = true; });
    connect(activityPlot_, &QCustomPlot::mouseWheel, this,
            [this](QWheelEvent*) { activityRangeLocked_ = true; });
    connect(throughputPlot_, &QCustomPlot::mousePress, this,
            [this](QMouseEvent*) { throughputRangeLocked_ = true; });
    connect(throughputPlot_, &QCustomPlot::mouseWheel, this,
            [this](QWheelEvent*) { throughputRangeLocked_ = true; });
    layout->addWidget(chartTabs, 1);
    return panel;
}

void MainWindow::resetSession() {
    if (batchTimer_) batchTimer_->stop();
    if (replayTimer_) replayTimer_->stop();
    if (replayPlayButton_) replayPlayButton_->setText(QStringLiteral("Play"));
    batchCancelRequested_ = true;
    pendingOrders_.clear();
    nextOrderIndex_ = 0;
    if (!engineReady_ || batchInFlight_ || engineRequestPending_) return;
    engineRequestPending_ = true;
    resetRequestID_ = nextRequestID_++;
    setOrderActionsEnabled(false);
    engineWorker_->reset(fullSystemMode_, resetRequestID_);
    submissionCalls_ = 0;
    acceptedOrders_ = 0;
    displayedTradeCount_ = 0;
    nextTimestamp_ = 1;
    latencyPoints_.clear();
    latencyRangeLocked_ = false;
    activityRangeLocked_ = false;
    throughputRangeLocked_ = false;
    plottedActivitySubmission_ = 0;
    replayEventCount_ = 0;
    replayCursor_ = 0;
    replayEventPageRequestID_ = 0;
    replayTable_->setRowCount(0);
    eventTable_->setRowCount(0);
    replayEventPageLabel_->setText(QStringLiteral("No events loaded"));
    replayEventPreviousButton_->setEnabled(false);
    replayEventNextButton_->setEnabled(false);
    if (replayProgress_) replayProgress_->setValue(0);
    metricsReportPath_.clear();
    orderHistoryTable_->setRowCount(0);
    loadProgress_->setValue(0);
    sessionStart_ = std::chrono::steady_clock::now();
    lastRateSampleTime_ = sessionStart_;
    previousRateSubmissionCalls_ = 0;
    previousRateAcceptedOrders_ = 0;
    previousRateTradeCount_ = 0;
    throughputSampleIndex_ = 0;
    if (replayStatus_) replayStatus_->setText(QStringLiteral("No event log loaded."));
    if (metricSummary_) {
        metricSummary_->setText(QStringLiteral(
            "Submission rates use session wall time (including idle time); use the CLI benchmark for capacity measurements."));
    }
    if (latencyPlot_) {
        latencyPlot_->graph(0)->data()->clear();
        latencyPlot_->replot();
    }
    if (activityPlot_) {
        for (int graph = 0; graph < activityPlot_->graphCount(); ++graph) {
            activityPlot_->graph(graph)->data()->clear();
        }
        activityPlot_->replot();
    }
    if (throughputPlot_) {
        for (int graph = 0; graph < throughputPlot_->graphCount(); ++graph) {
            throughputPlot_->graph(graph)->data()->clear();
        }
        throughputRangeLocked_ = false;
        throughputPlot_->xAxis->setRange(0, 60);
        throughputPlot_->yAxis->setRange(0, 10);
        throughputPlot_->replot();
    }
    bookSnapshot_ = BookSnapshot{};
    applyBookSnapshot(bookSnapshot_);
}

void MainWindow::submitOrder() {
    if (!engineReady_ || engineRequestPending_ || batchInFlight_) return;
    if (typeCombo_->currentIndex() == 0 && priceInput_->value() <= 0) {
        showError(QStringLiteral("Limit orders require a positive price."));
        return;
    }
    engineRequestPending_ = true;
    setOrderActionsEnabled(false);
    const OrderCommand command{
        idInput_->value(), priceInput_->value(), quantityInput_->value(),
        static_cast<long long>(nextTimestamp_++),
        sideCombo_->currentIndex() == 0 ? Side::Buy : Side::Sell,
        typeCombo_->currentIndex() == 0 ? OrderType::Limit : OrderType::Market};
    engineWorker_->submit(command, nextRequestID_++);
}

void MainWindow::cancelOrder() {
    if (!engineReady_ || engineRequestPending_ || batchInFlight_) return;
    const int id = idInput_->value();
    engineRequestPending_ = true;
    setOrderActionsEnabled(false);
    engineWorker_->cancel(id, nextRequestID_++);
}

void MainWindow::modifyOrder() {
    if (!engineReady_ || engineRequestPending_ || batchInFlight_) return;
    if (priceInput_->value() <= 0 || quantityInput_->value() <= 0) {
        showError(QStringLiteral("A modified resting order requires a positive price and quantity."));
        return;
    }
    engineRequestPending_ = true;
    setOrderActionsEnabled(false);
    engineWorker_->modify(idInput_->value(), priceInput_->value(),
                          quantityInput_->value(), nextRequestID_++);
}

void MainWindow::chooseOrderFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Load order requests"), QString(),
        QStringLiteral("Order data (*.json *.csv);;JSON files (*.json);;CSV files (*.csv)"));
    if (!path.isEmpty()) loadOrders(path);
}

void MainWindow::loadOrders(const QString& path) {
    if (!engineReady_ || engineRequestPending_) {
        showError(QStringLiteral("Wait for the engine to finish its current operation before loading orders."));
        return;
    }
    if (batchTimer_->isActive() || batchInFlight_) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Replace order batch"),
            QStringLiteral("Stop the current batch and load a new file?"));
        if (answer != QMessageBox::Yes) return;
        batchCancelRequested_ = true;
        batchTimer_->stop();
        if (batchInFlight_) {
            showError(QStringLiteral("The current batch chunk is finishing. Load the next file after it stops."));
            return;
        }
    }
    pendingOrders_.clear();
    nextOrderIndex_ = 0;
    engineRequestPending_ = true;
    orderParseRequestID_ = nextRequestID_++;
    setOrderActionsEnabled(false);
    loadProgress_->setValue(0);
    loadProgress_->setRange(0, 0);
    statusBar()->showMessage(QStringLiteral("Loading and validating order file..."));
    engineWorker_->parseOrders(path, orderParseRequestID_);
}

void MainWindow::processLoadedBatch() {
    if (!engineReady_ || batchInFlight_ || batchCancelRequested_ ||
        nextOrderIndex_ >= pendingOrders_.size()) return;
    constexpr std::size_t batchSize = 100;
    const std::size_t end = std::min(nextOrderIndex_ + batchSize, pendingOrders_.size());
    std::vector<OrderCommand> commands;
    commands.reserve(end - nextOrderIndex_);
    for (std::size_t index = nextOrderIndex_; index < end; ++index) {
        OrderCommand command = pendingOrders_[index];
        command.timestamp = static_cast<long long>(nextTimestamp_++);
        commands.push_back(command);
    }
    batchInFlight_ = true;
    batchTimer_->stop();
    setOrderActionsEnabled(false);
    cancelBatchButton_->setEnabled(true);
    engineWorker_->submitBatch(std::move(commands), nextRequestID_++);
}

void MainWindow::refreshBook() {
    applyBookSnapshot(bookSnapshot_);
}

void MainWindow::applyBookSnapshot(const BookSnapshot& snapshot) {
    if (!buyTree_ || !sellTree_) return;
    bookSnapshot_ = snapshot;
    if (snapshot.hasBestBid && snapshot.hasBestAsk) {
        bookSummary_->setText(QStringLiteral("BEST BID  %1     BEST ASK  %2     SPREAD  %3")
            .arg(snapshot.bestBid).arg(snapshot.bestAsk)
            .arg(snapshot.bestAsk - snapshot.bestBid));
    } else {
        bookSummary_->setText(QStringLiteral("BEST BID  %1     BEST ASK  %2")
            .arg(snapshot.hasBestBid ? QString::number(snapshot.bestBid) : QStringLiteral("-"))
            .arg(snapshot.hasBestAsk ? QString::number(snapshot.bestAsk) : QStringLiteral("-")));
    }
    buyTree_->clear();
    sellTree_->clear();
    appendLevel(buyTree_, nullptr, snapshot.buyLevels, snapshot.buyRoot);
    appendLevel(sellTree_, nullptr, snapshot.sellLevels, snapshot.sellRoot);
    graphicsScene_->clear();
    constexpr double treeGap = 130.0;
    const double buyWidth = drawBookTree(
        graphicsScene_, snapshot.buyLevels, snapshot.buyRoot, 0.0,
        sideColor(Side::Buy), Side::Buy);
    const double sellWidth = drawBookTree(
        graphicsScene_, snapshot.sellLevels, snapshot.sellRoot, buyWidth + treeGap,
        sideColor(Side::Sell), Side::Sell);
    const double sceneWidth = std::max(900.0, buyWidth + treeGap + sellWidth);
    const auto treeHeight = [](const std::vector<PriceLevelSnapshot>& levels, int root) {
        std::function<int(int)> height = [&](int index) {
            if (index < 0) return 0;
            const PriceLevelSnapshot& level = levels[static_cast<std::size_t>(index)];
            return 1 + std::max(height(level.left), height(level.right));
        };
        return height(root);
    };
    const int maxHeight = std::max(treeHeight(snapshot.buyLevels, snapshot.buyRoot),
                                   treeHeight(snapshot.sellLevels, snapshot.sellRoot));
    graphicsScene_->setSceneRect(0, 0, sceneWidth,
        std::max(380.0, 120.0 + maxHeight * 105.0));
    ordersTable_->setRowCount(static_cast<int>(snapshot.openOrders.size()));
    orderPageOrdersTable_->setRowCount(static_cast<int>(snapshot.openOrders.size()));
    for (std::size_t row = 0; row < snapshot.openOrders.size(); ++row) {
        const OrderSnapshot& order = snapshot.openOrders[row];
        const QString values[] = {QString::number(order.id), sideName(order.side),
            QString::number(order.price), QString::number(order.quantity),
            QString::number(order.fifoPosition)};
        for (int column = 0; column < 5; ++column) {
            ordersTable_->setItem(static_cast<int>(row), column, tableItem(values[column]));
            orderPageOrdersTable_->setItem(static_cast<int>(row), column, tableItem(values[column]));
        }
        const QColor tint = sideColor(order.side);
        ordersTable_->item(static_cast<int>(row), 1)->setForeground(tint);
        orderPageOrdersTable_->item(static_cast<int>(row), 1)->setForeground(tint);
    }

    tradesTable_->setRowCount(static_cast<int>(snapshot.trades.size()));
    orderPageTradesTable_->setRowCount(static_cast<int>(snapshot.trades.size()));
    for (std::size_t row = 0; row < snapshot.trades.size(); ++row) {
        const Trade& trade = snapshot.trades[row];
        const QString values[] = {QString::number(trade.tradeID),
            QString::number(trade.buyOrderID), QString::number(trade.sellOrderID),
            QString::number(trade.price), QString::number(trade.quantity)};
        for (int column = 0; column < 5; ++column) {
            tradesTable_->setItem(static_cast<int>(row), column, tableItem(values[column]));
            orderPageTradesTable_->setItem(static_cast<int>(row), column, tableItem(values[column]));
        }
        tradesTable_->item(static_cast<int>(row), 1)->setForeground(sideColor(Side::Buy));
        tradesTable_->item(static_cast<int>(row), 2)->setForeground(sideColor(Side::Sell));
        orderPageTradesTable_->item(static_cast<int>(row), 1)->setForeground(sideColor(Side::Buy));
        orderPageTradesTable_->item(static_cast<int>(row), 2)->setForeground(sideColor(Side::Sell));
        if (row >= displayedTradeCount_ && !reducedMotion_) {
            for (int col = 0; col < tradesTable_->columnCount(); ++col) {
                tradesTable_->item(static_cast<int>(row), col)->setBackground(QColor("#a6e3a1"));
            }
            auto* animation = new QVariantAnimation(this);
            animation->setDuration(700);
            animation->setStartValue(QColor("#a6e3a1"));
            animation->setEndValue(QColor("#1e1e2e"));
            animation->setEasingCurve(QEasingCurve::OutCubic);
            const int animatedRow = static_cast<int>(row);
            const QString tradeId = QString::number(snapshot.trades[row].tradeID);
            connect(animation, &QVariantAnimation::valueChanged, this,
                    [this, animatedRow, tradeId](const QVariant& value) {
                        if (animatedRow >= tradesTable_->rowCount() ||
                            !tradesTable_->item(animatedRow, 0) ||
                            tradesTable_->item(animatedRow, 0)->text() != tradeId) return;
                        const QColor color = value.value<QColor>();
                        for (int column = 0; column < tradesTable_->columnCount(); ++column) {
                            tradesTable_->item(animatedRow, column)->setBackground(color);
                        }
                    });
            connect(animation, &QVariantAnimation::finished, animation, &QObject::deleteLater);
            animation->start();
        }
    }
    displayedTradeCount_ = snapshot.trades.size();
    queueTable_->setRowCount(0);
    queueSummary_->setText(QStringLiteral("Select a rendered AVL node or price level to inspect its FIFO queue."));
    queueSummary_->setStyleSheet(QString());
    updateMetricsDashboard();
}

void MainWindow::setOrderActionsEnabled(bool enabled) {
    const bool ready = enabled && engineReady_ && !engineRequestPending_ && !batchInFlight_;
    submitOrderButton_->setEnabled(ready);
    cancelOrderButton_->setEnabled(ready);
    modifyOrderButton_->setEnabled(ready);
    loadOrderButton_->setEnabled(ready);
    modeCombo_->setEnabled(ready);
    resetBookButton_->setEnabled(ready && !batchTimer_->isActive());
    cancelBatchButton_->setEnabled(batchTimer_->isActive() || batchInFlight_);
}

void MainWindow::handleEngineReply(const EngineReply& reply) {
    if (reply.requestID == orderParseRequestID_) {
        orderParseRequestID_ = 0;
        engineRequestPending_ = false;
        if (!reply.success) {
            pendingOrders_.clear();
            loadProgress_->setRange(0, 100);
            loadProgress_->setValue(0);
            setOrderActionsEnabled(true);
            showError(reply.message);
            return;
        }
        pendingOrders_ = reply.parsedOrders;
        if (!reply.parseWarnings.empty()) {
            QString details;
            const std::size_t shown = std::min<std::size_t>(reply.parseWarnings.size(), 20);
            for (std::size_t index = 0; index < shown; ++index) {
                details += reply.parseWarnings[index] + QLatin1Char('\n');
            }
            if (reply.parseWarnings.size() > shown) {
                details += QStringLiteral("... and %1 more parser rejection(s).")
                    .arg(reply.parseWarnings.size() - shown);
            }
            QMessageBox::warning(this, QStringLiteral("Invalid order records"), details);
        }
        if (pendingOrders_.empty()) {
            loadProgress_->setRange(0, 100);
            loadProgress_->setValue(0);
            setOrderActionsEnabled(true);
            showError(QStringLiteral("The input contains no order records to process."));
            return;
        }
        nextOrderIndex_ = 0;
        loadProgress_->setRange(0, static_cast<int>(
            std::min<std::size_t>(pendingOrders_.size(),
                                  static_cast<std::size_t>(std::numeric_limits<int>::max()))));
        loadProgress_->setValue(0);
        batchCancelRequested_ = false;
        setOrderActionsEnabled(false);
        cancelBatchButton_->setEnabled(true);
        batchTimer_->start();
        statusBar()->showMessage(QStringLiteral("Loaded %1 requests; processing...")
                                     .arg(pendingOrders_.size()));
        return;
    }

    if (reply.requestID == replayLoadRequestID_) {
        replayLoadRequestID_ = 0;
        replayTimer_->stop();
        replayPlayButton_->setText(QStringLiteral("Play"));
        if (!reply.success) {
            replayStatus_->setStyleSheet(QStringLiteral("color: #f38ba8;"));
            replayStatus_->setText(reply.message);
            showError(reply.message);
            pendingReplayPath_.clear();
            return;
        }
        replayEventCount_ = reply.replayEventCount;
        replayCursor_ = 0;
        replayEventPageStart_ = 0;
        updateReplayProgress(0);
        replayTable_->setRowCount(0);
        replayStatus_->setStyleSheet(QString());
        displayReplayEventPage(reply);
        replayStatus_->setText(QStringLiteral(
            "Loaded %1 events from %2. Replay advances by incoming order; event rows are paged in blocks of 500.")
            .arg(replayEventCount_).arg(QFileInfo(pendingReplayPath_).fileName()));
        pendingReplayPath_.clear();
        return;
    }

    if (replayEventPageRequestID_ != 0 &&
        reply.requestID == replayEventPageRequestID_) {
        replayEventPageRequestID_ = 0;
        if (!reply.success) {
            replayStatus_->setStyleSheet(QStringLiteral("color: #f38ba8;"));
            replayStatus_->setText(reply.message);
            return;
        }
        displayReplayEventPage(reply);
        return;
    }

    if (reply.requestID == replayStepRequestID_) {
        replayStepRequestID_ = 0;
        if (!reply.success) {
            replayTimer_->stop();
            replayPlayButton_->setText(QStringLiteral("Play"));
            replayStatus_->setStyleSheet(QStringLiteral("color: #f38ba8;"));
            replayStatus_->setText(reply.message);
            return;
        }
        if (reply.hasReplaySnapshot) updateReplayView(reply.replaySnapshot);
        replayCursor_ = reply.replayCursor;
        updateReplayProgress(replayCursor_);
        replayStatus_->setStyleSheet(QString());
        replayStatus_->setText(QStringLiteral("%1 Progress: %2 / %3 events.")
            .arg(reply.message).arg(replayCursor_).arg(reply.replayEventCount));
        if (replayCursor_ >= replayEventCount_ && replayTimer_->isActive()) {
            replayTimer_->stop();
            replayPlayButton_->setText(QStringLiteral("Play"));
        }
        return;
    }

    if (reply.requestID == replayResetRequestID_) {
        replayResetRequestID_ = 0;
        if (!reply.success) {
            replayStatus_->setStyleSheet(QStringLiteral("color: #f38ba8;"));
            replayStatus_->setText(reply.message);
            return;
        }
        replayCursor_ = 0;
        updateReplayView(reply.replaySnapshot);
        updateReplayProgress(0);
        replayStatus_->setStyleSheet(QString());
        replayStatus_->setText(reply.message);
        return;
    }

    if (reply.requestID == replayFastForwardRequestID_) {
        replayFastForwardRequestID_ = 0;
        if (!reply.success) {
            replayStatus_->setStyleSheet(QStringLiteral("color: #f38ba8;"));
            replayStatus_->setText(reply.message);
            return;
        }
        replayCursor_ = reply.replayCursor;
        updateReplayView(reply.replaySnapshot);
        updateReplayProgress(replayCursor_);
        replayStatus_->setStyleSheet(QString());
        replayStatus_->setText(QStringLiteral("%1 Progress: %2 / %3 events.")
            .arg(reply.message).arg(replayCursor_).arg(reply.replayEventCount));
        return;
    }

    if (reply.requestID == replayFlushRequestID_) {
        replayFlushRequestID_ = 0;
        if (!reply.success) {
            showError(QStringLiteral("Could not flush the active event log: %1")
                          .arg(reply.message));
        } else {
            readReplayFile(pendingReplayPath_);
        }
        return;
    }

    if (reply.requestID == replayValidationRequestID_) {
        replayValidationRequestID_ = 0;
        replayTimer_->stop();
        replayPlayButton_->setText(QStringLiteral("Play"));
        if (reply.hasReplaySnapshot) {
            updateReplayView(reply.replaySnapshot);
            replayCursor_ = replayEventCount_;
            updateReplayProgress(replayCursor_);
        }
        replayStatus_->setStyleSheet(reply.success
            ? QStringLiteral("color: #a6e3a1;")
            : QStringLiteral("color: #f38ba8;"));
        if (reply.differences.empty()) {
            replayStatus_->setText(QStringLiteral(
                "%1 Through sequence %2.")
                .arg(reply.message).arg(reply.replaySequence));
        } else {
            QString report = QStringLiteral("VALIDATION FAILED - %1 difference(s):\n")
                .arg(reply.differences.size());
            const std::size_t shown = std::min<std::size_t>(reply.differences.size(), 12);
            for (std::size_t index = 0; index < shown; ++index) {
                report += QStringLiteral("- %1\n").arg(reply.differences[index]);
            }
            if (reply.differences.size() > shown) {
                report += QStringLiteral("... and %1 more.")
                    .arg(reply.differences.size() - shown);
            }
            replayStatus_->setText(report);
        }
        if (reply.fatalError) showError(reply.message);
        return;
    }

    if (reply.requestID == resetRequestID_) {
        engineRequestPending_ = false;
        if (!reply.success) {
            showError(QStringLiteral("Could not reset the engine: %1").arg(reply.message));
            if (fullSystemMode_) {
                fullSystemMode_ = false;
                modeCombo_->blockSignals(true);
                modeCombo_->setCurrentIndex(0);
                modeCombo_->blockSignals(false);
                engineRequestPending_ = true;
                resetRequestID_ = nextRequestID_++;
                engineWorker_->reset(false, resetRequestID_);
                statusBar()->showMessage(
                    QStringLiteral("Retrying in engine-only mode..."), 5000);
                return;
            }
            engineReady_ = false;
            setOrderActionsEnabled(false);
        } else {
            applyBookSnapshot(reply.snapshot);
            sessionStart_ = std::chrono::steady_clock::now();
        }
        setOrderActionsEnabled(true);
        return;
    }

    if (reply.batch) {
        batchInFlight_ = false;
        nextOrderIndex_ += reply.submittedCount;
        acceptedOrders_ += reply.acceptedCount;
        for (double latency : reply.latencyMs) {
            ++submissionCalls_;
            recordMetric(latency);
        }
        for (const OrderOutcomeSnapshot& outcome : reply.outcomes) {
            const int row = orderHistoryTable_->rowCount();
            orderHistoryTable_->insertRow(row);
            const QString values[] = {QString::number(outcome.id), sideName(outcome.side),
                outcome.type == OrderType::Limit ? QStringLiteral("LIMIT")
                                                 : QStringLiteral("MARKET"),
                QString::number(outcome.requestedQuantity),
                QString::number(outcome.executedQuantity),
                QString::number(outcome.remainingQuantity), outcome.reason};
            for (int column = 0; column < 7; ++column) {
                orderHistoryTable_->setItem(row, column, tableItem(values[column]));
            }
            orderHistoryTable_->item(row, 1)->setForeground(sideColor(outcome.side));
            if (!outcome.accepted) {
                orderHistoryTable_->item(row, 6)->setForeground(QColor("#f38ba8"));
            }
        }
        applyBookSnapshot(reply.snapshot);
        loadProgress_->setValue(static_cast<int>(nextOrderIndex_));
        if (reply.fatalError) {
            batchTimer_->stop();
            batchCancelRequested_ = true;
            showError(QStringLiteral("Order batch stopped: %1").arg(reply.message));
        }
        if (batchCancelRequested_ || nextOrderIndex_ >= pendingOrders_.size()) {
            batchTimer_->stop();
            const std::size_t completed = nextOrderIndex_;
            pendingOrders_.clear();
            nextOrderIndex_ = 0;
            batchCancelRequested_ = false;
            setOrderActionsEnabled(true);
            cancelBatchButton_->setEnabled(false);
            statusBar()->showMessage(
                reply.fatalError ? QStringLiteral("Order batch stopped after %1 requests.")
                                       .arg(completed)
                                 : QStringLiteral("Processed %1 order requests.")
                                       .arg(completed), 6000);
        } else {
            if (!reply.message.isEmpty()) statusBar()->showMessage(reply.message, 5000);
            cancelBatchButton_->setEnabled(true);
            batchTimer_->start();
        }
        return;
    }

    engineRequestPending_ = false;
    setOrderActionsEnabled(true);
    applyBookSnapshot(reply.snapshot);
    for (double latency : reply.latencyMs) {
        ++submissionCalls_;
        acceptedOrders_ += reply.acceptedCount;
        recordMetric(latency);
    }
    for (const OrderOutcomeSnapshot& outcome : reply.outcomes) {
        const int row = orderHistoryTable_->rowCount();
        orderHistoryTable_->insertRow(row);
        const QString values[] = {QString::number(outcome.id), sideName(outcome.side),
            outcome.type == OrderType::Limit ? QStringLiteral("LIMIT")
                                             : QStringLiteral("MARKET"),
            QString::number(outcome.requestedQuantity),
            QString::number(outcome.executedQuantity),
            QString::number(outcome.remainingQuantity), outcome.reason};
        for (int column = 0; column < 7; ++column) {
            orderHistoryTable_->setItem(row, column, tableItem(values[column]));
        }
        orderHistoryTable_->item(row, 1)->setForeground(sideColor(outcome.side));
        if (!outcome.accepted) {
            orderHistoryTable_->item(row, 6)->setForeground(QColor("#f38ba8"));
        }
    }
    if (!reply.message.isEmpty()) {
        statusBar()->showMessage(reply.message, reply.success ? 3000 : 8000);
    }
    if (reply.fatalError) showError(reply.message);
}

void MainWindow::appendLevel(QTreeWidget* tree, QTreeWidgetItem* parent,
                             const std::vector<PriceLevelSnapshot>& levels,
                             int index) {
    if (index < 0) return;
    const PriceLevelSnapshot& level = levels[static_cast<std::size_t>(index)];
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
    item->setText(0, QString::number(level.price));
    item->setText(1, QString::number(level.totalQuantity));
    item->setText(2, QString::number(level.orders.size()));
    item->setData(0, Qt::UserRole, level.price);
    item->setForeground(0, sideColor(tree == buyTree_ ? Side::Buy : Side::Sell));
    appendLevel(tree, item, levels, level.left);
    appendLevel(tree, item, levels, level.right);
}

const PriceLevelSnapshot* MainWindow::findLevel(
    const std::vector<PriceLevelSnapshot>& levels, int price) const {
    for (const PriceLevelSnapshot& level : levels) {
        if (level.price == price) return &level;
    }
    return nullptr;
}

void MainWindow::refreshQueue(QTreeWidget* tree, QTreeWidgetItem* item) {
    if (!item) return;
    refreshQueue(tree == buyTree_ ? Side::Buy : Side::Sell,
                 item->data(0, Qt::UserRole).toInt());
    bookTabs_->setCurrentWidget(queueTable_->parentWidget());
}

void MainWindow::refreshQueue(Side side, int price) {
    queueTable_->setRowCount(0);
    const auto& levels = side == Side::Buy ? bookSnapshot_.buyLevels
                                           : bookSnapshot_.sellLevels;
    const PriceLevelSnapshot* level = findLevel(levels, price);
    if (!level) return;
    queueSummary_->setText(QStringLiteral("%1 price level %2 - FIFO queue")
        .arg(sideName(side)).arg(price));
    queueSummary_->setStyleSheet(QStringLiteral("color: %1; font-weight: 600;")
        .arg(sideColor(side).name()));
    queueTable_->setRowCount(static_cast<int>(level->orders.size()));
    int row = 0;
    for (const OrderSnapshot& order : level->orders) {
        queueTable_->setItem(row, 0, tableItem(QString::number(row + 1)));
        queueTable_->setItem(row, 1, tableItem(QString::number(order.id)));
        queueTable_->setItem(row, 2, tableItem(QString::number(order.quantity)));
        queueTable_->setItem(row, 3, tableItem(QString::number(order.timestamp)));
        ++row;
    }
}

double MainWindow::drawBookTree(QGraphicsScene* scene,
                                const std::vector<PriceLevelSnapshot>& levels,
                                int root, double left, const QColor& color, Side side) {
    constexpr double horizontalGap = 150.0;
    constexpr double nodeWidth = 116.0;
    constexpr double nodeHeight = 58.0;
    constexpr double verticalGap = 105.0;
    const QString title = side == Side::Buy
        ? QStringLiteral("BUY AVL") : QStringLiteral("SELL AVL");
    auto* titleItem = scene->addText(title);
    titleItem->setDefaultTextColor(color);
    titleItem->setPos(left + 8.0, 5.0);
    if (root < 0) {
        auto* empty = scene->addText(QStringLiteral("No price levels"));
        empty->setDefaultTextColor(QColor("#a6adc8"));
        empty->setPos(left + 8.0, 35.0);
        return horizontalGap;
    }

    std::vector<std::size_t> subtreeSlots(levels.size(), 0);
    std::function<std::size_t(int)> countSlots =
        [&](int index) -> std::size_t {
            if (index < 0) return 0;
            const PriceLevelSnapshot& node = levels[static_cast<std::size_t>(index)];
            const std::size_t count = 1 + countSlots(node.left) + countSlots(node.right);
            subtreeSlots[static_cast<std::size_t>(index)] = count;
            return count;
        };
    const std::size_t nodeCount = countSlots(root);
    std::function<void(int, double, int)> layout =
        [&](int index, double subtreeLeft, int depth) {
            if (index < 0) return;
            const PriceLevelSnapshot& node = levels[static_cast<std::size_t>(index)];
            const std::size_t leftSlots = node.left >= 0
                ? subtreeSlots[static_cast<std::size_t>(node.left)] : 0;
            const double centerX = subtreeLeft +
                (static_cast<double>(leftSlots) + 0.5) * horizontalGap;
            const double centerY = 58.0 + depth * verticalGap;

            if (node.left >= 0) {
                const PriceLevelSnapshot& child =
                    levels[static_cast<std::size_t>(node.left)];
                const std::size_t childLeftSlots = child.left >= 0
                    ? subtreeSlots[static_cast<std::size_t>(child.left)] : 0;
                const double childX = subtreeLeft +
                    (static_cast<double>(childLeftSlots) + 0.5) * horizontalGap;
                const double childY = centerY + verticalGap;
                scene->addLine(centerX, centerY + nodeHeight / 2.0,
                               childX, childY - nodeHeight / 2.0,
                               QPen(QColor("#585b70"), 2));
                layout(node.left, subtreeLeft, depth + 1);
            }
            if (node.right >= 0) {
                const double rightLeft = subtreeLeft + (leftSlots + 1) * horizontalGap;
                const PriceLevelSnapshot& child =
                    levels[static_cast<std::size_t>(node.right)];
                const std::size_t childLeftSlots = child.left >= 0
                    ? subtreeSlots[static_cast<std::size_t>(child.left)] : 0;
                const double childX = rightLeft +
                    (static_cast<double>(childLeftSlots) + 0.5) * horizontalGap;
                const double childY = centerY + verticalGap;
                scene->addLine(centerX, centerY + nodeHeight / 2.0,
                               childX, childY - nodeHeight / 2.0,
                               QPen(QColor("#585b70"), 2));
                layout(node.right, rightLeft, depth + 1);
            }

            auto* nodeItem = scene->addEllipse(
                centerX - nodeWidth / 2.0, centerY - nodeHeight / 2.0,
                nodeWidth, nodeHeight, QPen(color, 2),
                QBrush(QColor("#313244")));
            nodeItem->setFlag(QGraphicsItem::ItemIsSelectable);
            nodeItem->setFlag(QGraphicsItem::ItemIsFocusable);
            nodeItem->setCursor(Qt::PointingHandCursor);
            nodeItem->setData(0, side == Side::Buy ? 0 : 1);
            nodeItem->setData(1, node.price);
            nodeItem->setToolTip(QStringLiteral(
                "%1 price %2\nTotal quantity: %3\nOrders: %4\nClick to view FIFO queue")
                .arg(title).arg(node.price).arg(node.totalQuantity)
                .arg(node.orders.size()));
            auto* priceText = scene->addText(QString::number(node.price));
            priceText->setDefaultTextColor(QColor("#cdd6f4"));
            priceText->setAcceptedMouseButtons(Qt::NoButton);
            priceText->setPos(centerX - priceText->boundingRect().width() / 2.0,
                              centerY - 21.0);
            auto* volumeText = scene->addText(QStringLiteral("%1 qty  /  %2 orders")
                .arg(node.totalQuantity).arg(node.orders.size()));
            volumeText->setDefaultTextColor(QColor("#a6adc8"));
            volumeText->setFont(QFont(QStringLiteral("Segoe UI"), 8));
            volumeText->setAcceptedMouseButtons(Qt::NoButton);
            volumeText->setPos(centerX - volumeText->boundingRect().width() / 2.0,
                               centerY + 1.0);
        };
    layout(root, left, 0);
    return nodeCount * horizontalGap;
}

void MainWindow::recordMetric(double latencyMs) {
    latencyPoints_.push_back(latencyMs);
    constexpr std::size_t sampleLimit = 5000;
    constexpr std::size_t trimCount = 1000;
    if (latencyPoints_.size() > sampleLimit) {
        latencyPoints_.erase(latencyPoints_.begin(),
                             latencyPoints_.begin() + trimCount);
    }
    const double x = static_cast<double>(submissionCalls_);
    latencyPlot_->graph(0)->addData(x, latencyMs);
    if (!batchTimer_->isActive() || submissionCalls_ % 100 == 0) {
        updateMetricsDashboard();
    }
}

void MainWindow::updateMetricsDashboard() {
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - sessionStart_).count();
    const double submissionsPerSecond =
        elapsed > 0.0 ? static_cast<double>(submissionCalls_) / elapsed : 0.0;
    const double acceptedPerSecond =
        elapsed > 0.0 ? static_cast<double>(acceptedOrders_) / elapsed : 0.0;
    const double tradesPerSecond = elapsed > 0.0
        ? static_cast<double>(bookSnapshot_.trades.size()) / elapsed : 0.0;
    std::vector<double> sortedLatency = latencyPoints_;
    std::sort(sortedLatency.begin(), sortedLatency.end());
    const auto percentile = [&sortedLatency](double fraction) {
        if (sortedLatency.empty()) return 0.0;
        const std::size_t index = std::min(
            sortedLatency.size() - 1,
            static_cast<std::size_t>(std::ceil(fraction * sortedLatency.size())) - 1);
        return sortedLatency[index];
    };
    const double meanLatency = sortedLatency.empty() ? 0.0 :
        std::accumulate(sortedLatency.begin(), sortedLatency.end(), 0.0) /
            sortedLatency.size();
    const std::uint64_t loggedEvents = bookSnapshot_.lastEventSequence;
    if (artifactPaths_) {
        artifactPaths_->setText(QStringLiteral(
            "Event log: %1  |  Metrics export: %2")
            .arg(QDir::current().filePath(QStringLiteral("lob_gui_events.log")))
            .arg(metricsReportPath_.isEmpty() ? QStringLiteral("not exported")
                                               : metricsReportPath_));
    }
    const double latestSubmission = static_cast<double>(submissionCalls_);
    latencyPlot_->graph(0)->data()->removeBefore(std::max(0.0, latestSubmission - 20000.0));
    if (!latencyRangeLocked_) {
        const double typicalLatency = sortedLatency.empty() ? 0.0 : percentile(0.99);
        latencyPlot_->xAxis->setRange(std::max(0.0, latestSubmission - 100.0),
                                      std::max(10.0, latestSubmission));
        latencyPlot_->yAxis->setRange(0.0, std::max(0.001, typicalLatency * 1.2));
    }

    if (submissionCalls_ > plottedActivitySubmission_) {
        const double eventCount = static_cast<double>(loggedEvents);
        activityPlot_->graph(0)->addData(latestSubmission, static_cast<double>(acceptedOrders_));
        activityPlot_->graph(1)->addData(
            latestSubmission, static_cast<double>(bookSnapshot_.trades.size()));
        activityPlot_->graph(2)->addData(latestSubmission, eventCount);
        plottedActivitySubmission_ = submissionCalls_;
        for (int graph = 0; graph < activityPlot_->graphCount(); ++graph) {
            activityPlot_->graph(graph)->data()->removeBefore(
                std::max(0.0, latestSubmission - 20000.0));
        }
        const double rateInterval = std::chrono::duration<double>(
            now - lastRateSampleTime_).count();
        if (rateInterval >= 1.0) {
            const double submittedRate = static_cast<double>(
                submissionCalls_ - previousRateSubmissionCalls_) / rateInterval;
            const double acceptedRate = static_cast<double>(
                acceptedOrders_ - previousRateAcceptedOrders_) / rateInterval;
            const std::size_t currentTradeCount = bookSnapshot_.trades.size();
            const std::size_t tradeDelta = currentTradeCount >= previousRateTradeCount_
                ? currentTradeCount - previousRateTradeCount_ : currentTradeCount;
            const double tradeRate = static_cast<double>(tradeDelta) / rateInterval;
            throughputPlot_->graph(0)->addData(
                static_cast<double>(throughputSampleIndex_), submittedRate);
            throughputPlot_->graph(1)->addData(
                static_cast<double>(throughputSampleIndex_), acceptedRate);
            throughputPlot_->graph(2)->addData(
                static_cast<double>(throughputSampleIndex_), tradeRate);
            ++throughputSampleIndex_;
            previousRateSubmissionCalls_ = submissionCalls_;
            previousRateAcceptedOrders_ = acceptedOrders_;
            previousRateTradeCount_ = bookSnapshot_.trades.size();
            lastRateSampleTime_ = now;
            throughputPlot_->graph(0)->data()->removeBefore(
                std::max(0.0, static_cast<double>(throughputSampleIndex_) - 300.0));
            throughputPlot_->graph(1)->data()->removeBefore(
                std::max(0.0, static_cast<double>(throughputSampleIndex_) - 300.0));
            throughputPlot_->graph(2)->data()->removeBefore(
                std::max(0.0, static_cast<double>(throughputSampleIndex_) - 300.0));
            throughputPlot_->replot(QCustomPlot::rpQueuedReplot);
        }
    }
    if (!activityRangeLocked_) {
        activityPlot_->xAxis->setRange(std::max(0.0, latestSubmission - 100.0),
                                       std::max(10.0, latestSubmission));
        activityPlot_->yAxis->setRange(0.0, std::max(
            10.0, std::max(static_cast<double>(acceptedOrders_),
                           std::max(static_cast<double>(bookSnapshot_.trades.size()),
                                    static_cast<double>(loggedEvents))) * 1.15));
    }
    if (!throughputRangeLocked_) {
        throughputPlot_->xAxis->setRange(
            std::max(0.0, static_cast<double>(throughputSampleIndex_) - 60.0),
            std::max(60.0, static_cast<double>(throughputSampleIndex_)));
        double maxRate = 10.0;
        for (int graph = 0; graph < throughputPlot_->graphCount(); ++graph) {
            bool foundRange = false;
            const QCPRange valueRange =
                throughputPlot_->graph(graph)->getValueRange(foundRange);
            if (foundRange) maxRate = std::max(maxRate, valueRange.upper);
        }
        throughputPlot_->yAxis->setRange(0.0, maxRate * 1.15);
    }
    latencyPlot_->replot(QCustomPlot::rpQueuedReplot);
    activityPlot_->replot(QCustomPlot::rpQueuedReplot);
    submittedMetricValue_->setText(QString::number(submissionCalls_));
    acceptedMetricValue_->setText(QString::number(acceptedOrders_));
    tradeMetricValue_->setText(QString::number(bookSnapshot_.trades.size()));
    latencyMetricValue_->setText(QStringLiteral("%1 ms").arg(meanLatency, 0, 'f', 4));
    percentileMetricValue_->setText(QStringLiteral("%1 / %2 ms")
        .arg(percentile(0.95), 0, 'f', 4).arg(percentile(0.99), 0, 'f', 4));
    eventMetricValue_->setText(QString::number(loggedEvents));
    metricSummary_->setText(QStringLiteral(
        "Mode: %1  |  Session rates: %2 submissions/s, %3 accepted/s, %4 trades/s"
        "  |  Latest: %5 ms\nLatency percentiles use the most recent %6 calls. Rates use session wall time including idle periods. Per-order latency covers the engine submit call%7.")
        .arg(fullSystemMode_ ? QStringLiteral("Full system (Worklet 3 enabled)")
                             : QStringLiteral("Engine only"))
        .arg(submissionsPerSecond, 0, 'f', 1)
        .arg(acceptedPerSecond, 0, 'f', 1)
        .arg(tradesPerSecond, 0, 'f', 1)
        .arg(latencyPoints_.empty() ? 0.0 : latencyPoints_.back(), 0, 'f', 4)
        .arg(latencyPoints_.size())
        .arg(fullSystemMode_ ? QStringLiteral("; includes event enqueue and feed publication/drain, excludes disk flush")
                             : QString()));

    if (!bookSnapshot_.feedActive) {
        feedSummary_->setText(QStringLiteral(
            "Worklet 3 feed: inactive in engine-only mode. Switch to Full system to collect event-log and feed diagnostics."));
        return;
    }
    const FeedStats& stats = bookSnapshot_.feedStats;
    const ConsumerStats& consumer = bookSnapshot_.consumerStats;
    const std::uint64_t missedEvents = consumer.missedEvents;
    feedSummary_->setText(QStringLiteral(
        "WORKLET 3 FEED   Published: %1   Read: %2   Sequence: %3   Missed: %4   Overflows: %5   Capacity: %6   Avg publish: %7 ns   Max: %8 ns")
        .arg(stats.eventsPublished)
        .arg(consumer.eventsRead)
        .arg(stats.latestSequence)
        .arg(missedEvents)
        .arg(stats.overflowCount)
        .arg(stats.capacity)
        .arg(stats.averagePublishTimeNs, 0, 'f', 1)
        .arg(stats.maxPublishTimeNs, 0, 'f', 1));
    feedSummary_->setStyleSheet(
        missedEvents > 0 || stats.overflowCount > 0
            ? QStringLiteral("color: #f38ba8;")
            : QStringLiteral("color: #94e2d5;"));
}

void MainWindow::exportMetrics() {
    if (!engineReady_ || engineRequestPending_ || batchInFlight_ ||
        batchTimer_->isActive()) {
        showError(QStringLiteral("Wait for the current engine operation or order batch to finish before exporting metrics."));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export run metrics"),
        QDir::current().filePath(QStringLiteral("lob_gui_metrics.csv")),
        QStringLiteral("CSV files (*.csv)"));
    if (path.isEmpty()) return;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(QStringLiteral("Could not create metrics export: %1")
                      .arg(file.errorString()));
        return;
    }
    QTextStream stream(&file);
    stream << "record_type,metric,value,unit,details\n";
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - sessionStart_).count();
    std::vector<double> sortedLatency = latencyPoints_;
    std::sort(sortedLatency.begin(), sortedLatency.end());
    const auto percentile = [&sortedLatency](double fraction) {
        if (sortedLatency.empty()) return 0.0;
        const std::size_t index = std::min(sortedLatency.size() - 1,
            static_cast<std::size_t>(std::ceil(fraction * sortedLatency.size())) - 1);
        return sortedLatency[index];
    };
    const double meanLatency = sortedLatency.empty() ? 0.0 :
        std::accumulate(sortedLatency.begin(), sortedLatency.end(), 0.0) /
            sortedLatency.size();
    const double submissionsPerSecond = elapsed > 0.0
        ? static_cast<double>(submissionCalls_) / elapsed : 0.0;
    const double acceptedPerSecond = elapsed > 0.0
        ? static_cast<double>(acceptedOrders_) / elapsed : 0.0;
    const double tradesPerSecond = elapsed > 0.0
        ? static_cast<double>(bookSnapshot_.trades.size()) / elapsed : 0.0;
    const auto writeRow = [&stream](const QStringList& values) {
        for (int column = 0; column < values.size(); ++column) {
            if (column != 0) stream << ',';
            stream << csvEscape(values[column]);
        }
        stream << '\n';
    };
    const auto metric = [&writeRow](const QString& name, double value,
                                    const QString& unit, const QString& details = QString()) {
        writeRow({QStringLiteral("summary"), name, QString::number(value, 'g', 12),
                  unit, details});
    };

    const QString mode = fullSystemMode_ ? QStringLiteral("full_system")
                                         : QStringLiteral("engine_only");
    metric(QStringLiteral("submission_calls"), static_cast<double>(submissionCalls_),
           QStringLiteral("orders"), mode);
    metric(QStringLiteral("accepted_orders"), static_cast<double>(acceptedOrders_),
           QStringLiteral("orders"), mode);
    metric(QStringLiteral("trade_executions"),
           static_cast<double>(bookSnapshot_.trades.size()),
           QStringLiteral("trades"), mode);
    metric(QStringLiteral("session_wall_time"), elapsed, QStringLiteral("seconds"),
           QStringLiteral("Includes idle time."));
    metric(QStringLiteral("submissions_per_second"), submissionsPerSecond,
           QStringLiteral("orders/s"),
           QStringLiteral("Valid submissions including rejected engine requests."));
    metric(QStringLiteral("accepted_orders_per_second"), acceptedPerSecond,
           QStringLiteral("orders/s"));
    metric(QStringLiteral("trade_executions_per_second"), tradesPerSecond,
           QStringLiteral("trades/s"));
    metric(QStringLiteral("latency_sample_count"),
           static_cast<double>(sortedLatency.size()),
           QStringLiteral("samples"),
           QStringLiteral("Recent in-memory sample window; max 5000 calls."));
    metric(QStringLiteral("latency_mean"), meanLatency, QStringLiteral("ms"));
    metric(QStringLiteral("latency_p50"), percentile(0.50), QStringLiteral("ms"));
    metric(QStringLiteral("latency_p95"), percentile(0.95), QStringLiteral("ms"));
    metric(QStringLiteral("latency_p99"), percentile(0.99), QStringLiteral("ms"));
    metric(QStringLiteral("latency_max"), sortedLatency.empty() ? 0.0 : sortedLatency.back(),
           QStringLiteral("ms"));
    metric(QStringLiteral("last_event_sequence"),
           static_cast<double>(bookSnapshot_.lastEventSequence),
           QStringLiteral("events"),
           QDir::current().filePath(QStringLiteral("lob_gui_events.log")));
    if (bookSnapshot_.feedActive) {
        metric(QStringLiteral("feed_events_published"),
               static_cast<double>(bookSnapshot_.feedStats.eventsPublished),
               QStringLiteral("events"));
        metric(QStringLiteral("feed_events_missed"),
               static_cast<double>(bookSnapshot_.consumerStats.missedEvents),
               QStringLiteral("events"));
        metric(QStringLiteral("feed_overflows"),
               static_cast<double>(bookSnapshot_.feedStats.overflowCount),
               QStringLiteral("overflows"));
        metric(QStringLiteral("feed_average_publish_time"),
               bookSnapshot_.feedStats.averagePublishTimeNs, QStringLiteral("ns"));
        metric(QStringLiteral("feed_max_publish_time"),
               bookSnapshot_.feedStats.maxPublishTimeNs, QStringLiteral("ns"));
    }
    writeRow({QStringLiteral("definition"), QStringLiteral("timing_boundary"), QString(),
        QString(), fullSystemMode_
            ? QStringLiteral("Per-order latency covers submit, event publication and feed drain; excludes disk flush.")
            : QStringLiteral("Per-order latency covers the engine submit call; Worklet 3 is inactive.")});
    writeRow({QStringLiteral("definition"), QStringLiteral("rates"), QString(),
        QString(), QStringLiteral("Session wall time includes idle periods; use CLI benchmark for capacity measurements.")});
    writeRow({QStringLiteral("record_type"), QStringLiteral("submission_index"),
              QStringLiteral("latency_ms"), QStringLiteral("unit"), QStringLiteral("run_mode")});
    const std::size_t firstSample =
        submissionCalls_ > latencyPoints_.size()
            ? submissionCalls_ - latencyPoints_.size() + 1 : 1;
    for (std::size_t index = 0; index < latencyPoints_.size(); ++index) {
        writeRow({QStringLiteral("latency_sample"),
            QString::number(firstSample + index),
            QString::number(latencyPoints_[index], 'g', 12),
            QStringLiteral("ms"), mode});
    }
    if (!file.commit()) {
        showError(QStringLiteral("Could not finalize metrics export: %1")
                      .arg(file.errorString()));
        return;
    }
    metricsReportPath_ = QFileInfo(path).absoluteFilePath();
    updateMetricsDashboard();
    statusBar()->showMessage(QStringLiteral("Exported run metrics to %1")
                                 .arg(metricsReportPath_), 7000);
}

void MainWindow::openCurrentEventLog() {
    if (engineRequestPending_ || batchInFlight_ || batchTimer_->isActive()) {
        showError(QStringLiteral("Wait for the current engine operation or order batch to finish before opening its event log."));
        return;
    }
    const QString path = QDir::current().filePath(QStringLiteral("lob_gui_events.log"));
    if (!QFileInfo::exists(path)) {
        showError(QStringLiteral("No event log exists at %1. Enable Full system mode and submit orders first.")
                      .arg(path));
        return;
    }
    loadReplay(path);
    replayNavButton_->click();
}

void MainWindow::chooseReplayFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open event log"), QString(),
        QStringLiteral("Event logs (*.log *.txt);;All files (*)"));
    if (!path.isEmpty()) loadReplay(path);
}

void MainWindow::loadReplay(const QString& path) {
    if (replayLoadRequestID_ != 0 || replayFlushRequestID_ != 0 ||
        replayValidationRequestID_ != 0 || replayStepRequestID_ != 0 ||
        replayEventPageRequestID_ != 0) return;
    replayTimer_->stop();
    replayPlayButton_->setText(QStringLiteral("Play"));
    replayEventCount_ = 0;
    replayCursor_ = 0;
    replayEventPageRequestID_ = 0;
    updateReplayProgress(0);
    replayTable_->setRowCount(0);
    eventTable_->setRowCount(0);
    replayEventPageStart_ = 0;
    replayEventPageLabel_->setText(QStringLiteral("No events loaded"));
    replayEventPreviousButton_->setEnabled(false);
    replayEventNextButton_->setEnabled(false);
    if (fullSystemMode_) {
        pendingReplayPath_ = path;
        replayFlushRequestID_ = nextRequestID_++;
        engineWorker_->flushEventLog(replayFlushRequestID_);
        replayStatus_->setText(QStringLiteral("Flushing the active event log..."));
        return;
    }
    readReplayFile(path);
}

void MainWindow::readReplayFile(const QString& path) {
    pendingReplayPath_ = path;
    replayLoadRequestID_ = nextRequestID_++;
    engineWorker_->loadReplay(path, replayLoadRequestID_);
    replayStatus_->setText(QStringLiteral("Loading and validating the event log..."));
}

void MainWindow::loadReplayEventPage(int page) {
    if (replayEventPageRequestID_ != 0 || replayEventCount_ == 0 ||
        page < 0 || replayLoadRequestID_ != 0 || replayFlushRequestID_ != 0) return;
    replayEventPageRequestID_ = nextRequestID_++;
    replayEventPreviousButton_->setEnabled(false);
    replayEventNextButton_->setEnabled(false);
    engineWorker_->loadReplayEventPage(static_cast<std::size_t>(page) * 500, 500,
                                       replayEventPageRequestID_);
}

void MainWindow::displayReplayEventPage(const EngineReply& reply) {
    replayEventPageStart_ = reply.replayEventPageStart;
    eventTable_->setRowCount(static_cast<int>(reply.events.size()));
    for (std::size_t row = 0; row < reply.events.size(); ++row) {
        const ReplayEventSnapshot& event = reply.events[row];
        const QString participant = event.type == EventType::TRADE
            ? QStringLiteral("%1 / %2").arg(event.buyOrderID).arg(event.sellOrderID)
            : QStringLiteral("-");
        eventTable_->setItem(static_cast<int>(row), 0,
            tableItem(QString::number(event.sequenceID)));
        eventTable_->setItem(static_cast<int>(row), 1,
            tableItem(eventTypeName(event.type)));
        eventTable_->setItem(static_cast<int>(row), 2,
            tableItem(event.type == EventType::TRADE
                ? QStringLiteral("-") : sideName(event.side)));
        if (event.type != EventType::TRADE) {
            eventTable_->item(static_cast<int>(row), 2)->setForeground(sideColor(event.side));
        }
        eventTable_->setItem(static_cast<int>(row), 3,
            tableItem(event.type == EventType::TRADE ? QStringLiteral("-")
                                                      : QString::number(event.orderID)));
        eventTable_->setItem(static_cast<int>(row), 4, tableItem(participant));
        eventTable_->setItem(static_cast<int>(row), 5, tableItem(QString::number(event.price)));
        eventTable_->setItem(static_cast<int>(row), 6, tableItem(QString::number(event.quantity)));
    }
    const std::size_t first = reply.events.empty() ? 0 : replayEventPageStart_ + 1;
    const std::size_t last = replayEventPageStart_ + reply.events.size();
    replayEventPageLabel_->setText(QStringLiteral("Events %1-%2 of %3")
        .arg(first).arg(last).arg(reply.replayEventCount));
    replayEventPreviousButton_->setEnabled(replayEventPageStart_ > 0);
    replayEventNextButton_->setEnabled(last < reply.replayEventCount);
}

void MainWindow::validateReplayAgainstLive() {
    if (replayEventCount_ == 0 || replayValidationRequestID_ != 0 ||
        replayLoadRequestID_ != 0 || replayFlushRequestID_ != 0 ||
        replayStepRequestID_ != 0) {
        if (replayEventCount_ == 0) {
            showError(QStringLiteral("Open an event log before validating."));
        }
        return;
    }
    replayTimer_->stop();
    replayPlayButton_->setText(QStringLiteral("Play"));
    replayStatus_->setStyleSheet(QString());
    replayStatus_->setText(QStringLiteral("Reconstructing and comparing on the engine worker..."));
    replayValidationRequestID_ = nextRequestID_++;
    engineWorker_->validateReplay(replayValidationRequestID_);
}

void MainWindow::appendReplayStep() {
    if (replayEventCount_ == 0 || replayStepRequestID_ != 0 ||
        replayLoadRequestID_ != 0 || replayFlushRequestID_ != 0 ||
        replayValidationRequestID_ != 0 || replayResetRequestID_ != 0 ||
        replayFastForwardRequestID_ != 0) {
        if (replayEventCount_ == 0) {
            showError(QStringLiteral("Load a valid event log before replaying."));
        }
        return;
    }
    replayStepRequestID_ = nextRequestID_++;
    replayStatus_->setText(QStringLiteral("Reconstructing the next order boundary..."));
    engineWorker_->stepReplay(replayStepRequestID_);
}

void MainWindow::resetReplay() {
    if (replayEventCount_ == 0 || replayResetRequestID_ != 0 ||
        replayStepRequestID_ != 0 || replayFastForwardRequestID_ != 0 ||
        replayLoadRequestID_ != 0 || replayFlushRequestID_ != 0 ||
        replayValidationRequestID_ != 0) return;
    replayTimer_->stop();
    replayPlayButton_->setText(QStringLiteral("Play"));
    replayResetRequestID_ = nextRequestID_++;
    engineWorker_->resetReplay(replayResetRequestID_);
}

void MainWindow::fastForwardReplay() {
    if (replayEventCount_ == 0 || replayFastForwardRequestID_ != 0 ||
        replayStepRequestID_ != 0 || replayResetRequestID_ != 0 ||
        replayLoadRequestID_ != 0 || replayFlushRequestID_ != 0 ||
        replayValidationRequestID_ != 0) return;
    replayTimer_->stop();
    replayPlayButton_->setText(QStringLiteral("Play"));
    replayFastForwardRequestID_ = nextRequestID_++;
    replayStatus_->setText(QStringLiteral("Fast-forwarding on the replay worker..."));
    engineWorker_->fastForwardReplay(replayFastForwardRequestID_);
}

void MainWindow::updateReplayProgress(std::size_t cursor) {
    if (!replayProgress_) return;
    const int progress = replayEventCount_ == 0 ? 0 : static_cast<int>(
        (1000.0 * static_cast<double>(cursor)) /
        static_cast<double>(replayEventCount_));
    replayProgress_->setValue(std::clamp(progress, 0, 1000));
    replayProgress_->setFormat(QStringLiteral("%1 / %2 events")
        .arg(cursor).arg(replayEventCount_));
}

void MainWindow::updateReplayView(const ReplaySnapshot& snapshot) {
    replayTable_->setRowCount(static_cast<int>(snapshot.levels.size()));
    for (std::size_t row = 0; row < snapshot.levels.size(); ++row) {
        const ReplayLevel& level = snapshot.levels[row];
        QStringList ids;
        for (int id : level.orderIDs) ids.push_back(QString::number(id));
        replayTable_->setItem(static_cast<int>(row), 0, tableItem(sideName(level.side)));
        replayTable_->item(static_cast<int>(row), 0)->setForeground(sideColor(level.side));
        replayTable_->setItem(static_cast<int>(row), 1, tableItem(QString::number(level.price)));
        replayTable_->setItem(static_cast<int>(row), 2, tableItem(QString::number(level.totalQuantity)));
        replayTable_->setItem(static_cast<int>(row), 3, tableItem(ids.join(QStringLiteral(", "))));
    }
}

void MainWindow::chooseCsvFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open CSV"), QString(), QStringLiteral("CSV files (*.csv);;All files (*)"));
    if (!path.isEmpty()) loadCsv(path);
}

void MainWindow::loadCsv(const QString& path) {
    csvPath_ = path;
    csvPage_ = 0;
    csvTotalRows_ = 0;
    csvFilter_ = csvFilterInput_->text();
    loadCsvPage();
}

void MainWindow::loadCsvPage() {
    if (!csvWorker_ || csvPath_.isEmpty()) return;
    csvRequestID_ = nextCsvRequestID_++;
    csvProgress_->setValue(0);
    csvStatus_->setText(QStringLiteral("Indexing/searching CSV in the background..."));
    csvWorker_->loadPage(csvPath_, csvFilter_, csvPage_, 500, csvRequestID_);
}

void MainWindow::updateCsvPage(const CsvPageReply& reply) {
    if (reply.requestID != csvRequestID_) return;
    if (!reply.success) {
        csvStatus_->setText(reply.error);
        csvProgress_->setValue(0);
        csvPreviousButton_->setEnabled(false);
        csvNextButton_->setEnabled(false);
        showError(reply.error);
        return;
    }
    csvTotalRows_ = reply.totalRows;
    csvModel_->setPage(reply.headers, reply.rows);
    csvProgress_->setValue(100);
    const std::size_t pageCount = std::max<std::size_t>(
        1, (csvTotalRows_ + 499) / 500);
    csvPageLabel_->setText(QStringLiteral("Page %1 of %2")
        .arg(csvPage_ + 1).arg(pageCount));
    csvPreviousButton_->setEnabled(csvPage_ > 0);
    csvNextButton_->setEnabled(
        static_cast<std::size_t>(csvPage_ + 1) < pageCount);
    csvStatus_->setText(QStringLiteral("%1 matching rows in %2; showing up to 500. Search scans the entire file; sort applies to this page.")
        .arg(csvTotalRows_).arg(QFileInfo(csvPath_).fileName()));
}

void MainWindow::showError(const QString& message) {
    QMessageBox::critical(this, QStringLiteral("Limit Order Book"), message);
}
