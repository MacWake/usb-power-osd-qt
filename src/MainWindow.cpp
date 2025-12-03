#include "MainWindow.h"

#include "AboutDialog.h"
#include "DeviceSelectionDialog.h"
#include <QApplication>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>
#include <QDebug>

MainWindow::MainWindow(OsdSettings *settings,
                       QWidget *parent) // NOLINT(*-pro-type-member-init)
    : QMainWindow(parent), settings(settings),
      m_powerMonitor(new PowerMonitor(this)),
      m_deviceManager(new DeviceManager(this)),
      m_settingsdialog(new SettingsDialog(settings, this)),
      m_history(new MeasurementHistory(1000)), // todo change hard coded value
      m_updateTimer(new QTimer(this)), m_statusBarHideTimer(new QTimer(this)),
      m_deviceSelectionDialog(nullptr) {
    this->m_currentGraph = new CurrentGraph(this, m_history, settings);
    this->m_deviceManager->setSettings(settings);
    statusBar()->setVisible(false);

    if (MainWindow::settings->window_width > 0 &&
        MainWindow::settings->window_height > 0) {
        setGeometry(MainWindow::settings->window_left,
                    MainWindow::settings->window_top,
                    MainWindow::settings->window_width,
                    MainWindow::settings->window_height);
    }
    setupUI();


    // Connect signals
    connect(m_deviceManager, &DeviceManager::powerDataReceived, this,
            &MainWindow::onPowerDataReceived);
    connect(m_deviceManager, &DeviceManager::deviceConnected, this,
            &MainWindow::onDeviceConnected);
    connect(m_deviceManager, &DeviceManager::deviceDisconnected, this,
            &MainWindow::onDeviceDisconnected);

    // Setup timers
    m_updateTimer->setInterval(200);
    connect(m_updateTimer, &QTimer::timeout, [this]() { this->updateLabels(); });
    m_statusBarHideTimer->setSingleShot(true); // Only fire once
    connect(m_statusBarHideTimer, &QTimer::timeout, this,
            &MainWindow::hideStatusBar);

    QTimer::singleShot(50, [this] { MainWindow::connectLastDevice(false); });

    this->m_reconnect_timer = new QTimer(this);
    this->m_reconnect_timer->setInterval(1000);
    connect(this->m_reconnect_timer, &QTimer::timeout,
            [this] { this->connectLastDevice(true); });
    // } else {
    //   // Start scanning with last known device settings
    //   m_deviceManager->startScanning();
    //   statusBar()->showMessage("Scanning for USB Power devices...");
    // }
}

MainWindow::~MainWindow() = default;

void MainWindow::startReconnectTimer() const { this->m_reconnect_timer->start(); }

void MainWindow::showStatusMessage(const QString &message,
                                   int hideAfterMs = 5000) {
    statusBar()->setVisible(true);
    statusBar()->showMessage(message);

    if (centralWidget()) {
        centralWidget()->updateGeometry();
        update();
    }

    // Restart the timer (this will cancel any previous timer)
    m_statusBarHideTimer->start(hideAfterMs);
}

void MainWindow::hideStatusBar() {
    statusBar()->setVisible(false);

    // Force the central widget to use the full available space
    if (centralWidget()) {
        // Get the current window size
        QSize windowSize = size();

        // Calculate the new geometry for the central widget
        // (accounting for menu bar but not status bar)
        int menuBarHeight = menuBar()->isVisible() ? menuBar()->height() : 0;
        QRect newGeometry(0, menuBarHeight, windowSize.width(),
                          windowSize.height() - menuBarHeight);

        centralWidget()->setGeometry(newGeometry);

        // Now reposition our widgets within the expanded central widget
        positionWidgets();
    }
}

void MainWindow::connectLastDevice(bool reconnecting = false) {
    // qDebug() << "Trying to connect to last device...
    // (reconnect="<<reconnecting<<")";
    if (!settings->last_device.isEmpty()) {
        // qDebug() << "Trying to connect to last device " << settings->last_device;
        if (this->m_deviceManager->tryConnect(settings->last_device)) {
            if (reconnecting) {
                this->m_reconnect_timer->stop();
            }
            return;
        } else {
            // qDebug() << "Failed to connect to last device " <<
            // settings->last_device;
        }
    } else {
        // qDebug() << "No last device found";
    }
    if (!reconnecting) {
        this->showDeviceSelectionDialog();
    }
}

void MainWindow::toggleEnergy() const {
    this->settings->is_energy_displayed = !this->settings->is_energy_displayed;
    this->settings->saveSettings();
    this->lblEnergy->setVisible(this->settings->is_energy_displayed);
}

void MainWindow::showDeviceSelectionDialog() {
    this->m_reconnect_timer->stop();
    if (!m_deviceSelectionDialog) {
        m_deviceSelectionDialog = new DeviceSelectionDialog(this);
    }

    if (m_deviceSelectionDialog->exec() == QDialog::Accepted) {
        // qDebug() << "Accepted DeviceSelectionDialog";
        auto connectionType = m_deviceSelectionDialog->getSelectedConnectionType();

        // qDebug() << "Selected connection type: " <<
        // static_cast<int>(connectionType);
        if (connectionType ==
            DeviceSelectionDialog::ConnectionType::BluetoothAuto) {
            // qDebug() << "Bluetooth auto discovery is enabled";

            statusBar()->showMessage("Scanning for Bluetooth devices...");
            m_deviceManager->startBtScanning();
            this->settings->last_device = "ble";
            this->settings->saveSettings();
        } else if (connectionType ==
                   DeviceSelectionDialog::ConnectionType::SerialPort) {
            // qDebug() << "Selected serial port; stopping/disconnecting ble device";
            m_deviceManager->stopBtScanning();
            QString selectedPort = m_deviceSelectionDialog->getSelectedSerialPort();
            // qDebug() << "Selected serial port: " << selectedPort;
            this->settings->last_device = selectedPort;
            this->settings->saveSettings();
            statusBar()->showMessage(
                QString("Connecting to %1...").arg(selectedPort));
            // qDebug() << "tryConnect() to " << selectedPort;
            if (!m_deviceManager->tryConnect(selectedPort)) {
                // qDebug() << "tryConnect() failed for " << selectedPort;
                statusBar()->showMessage("Failed to connect to " + selectedPort);
            } else {
                // qDebug() << "tryConnect() succeeded for " << selectedPort;
                return;
            }
        } else {
            // qDebug() << "No connection type selected";
            return;
        }
    } else {
        // User cancelled
        // statusBar()->showMessage("No device selected.");
        // QTimer::singleShot(2000, QApplication::instance(), &QApplication::quit);
    }
    // QTimer::singleShot(100, [this] { MainWindow::connectLastDevice(false); });
}

// ReSharper disable CppDFAMemoryLeak
void MainWindow::setupUI() {
    setWindowTitle("MacWake USB Power OSD");

    setBackgroundColor(settings->color_bg);
    this->lblVoltage = new QLabel("");
    this->lblCurrent = new QLabel("");
    this->lblPower = new QLabel("");
    this->lblEnergy = new QLabel("");
    this->lblMinMaxCurrent = new QLabel("");
    this->fntPrimary = QFont(this->settings->primary_font_name,
                             this->settings->primary_font_size);
    this->fntSecondary = QFont(this->settings->secondary_font_name,
                               this->settings->secondary_font_size);
    this->lblVoltage->setFont(fntPrimary);
    this->lblCurrent->setFont(fntPrimary);
    this->lblPower->setFont(fntSecondary);
    this->lblEnergy->setFont(fntSecondary);
    this->lblMinMaxCurrent->setFont(fntSecondary);

    this->lblVoltage->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");
    this->lblCurrent->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");
    this->lblPower->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");
    this->lblEnergy->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");
    this->lblMinMaxCurrent->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");

    this->lblVoltage->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    this->lblCurrent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    this->lblPower->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    this->lblEnergy->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    this->lblMinMaxCurrent->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    this->lblMinMaxCurrent->setAttribute(Qt::WA_Hover, true);
    this->lblMinMaxCurrent->installEventFilter(this);
    this->lblMinMaxCurrent->setCursor(Qt::PointingHandCursor);
    this->lblMinMaxCurrent->setToolTip("Double-click to reset min/max history");

    // Create central widget without layout - we'll position manually
    auto *centralWidget = new QWidget;
    setCentralWidget(centralWidget);

    // Set all widgets to have the central widget as parent for manual positioning
    lblVoltage->setParent(centralWidget);
    lblCurrent->setParent(centralWidget);
    lblPower->setParent(centralWidget);
    lblEnergy->setParent(centralWidget);
    lblEnergy->setVisible(this->settings->is_energy_displayed);
    lblMinMaxCurrent->setParent(centralWidget);
    m_currentGraph->setParent(centralWidget);

    auto toggleEnergyAction = new QAction("Toggle Energy", this);
    toggleEnergyAction->setShortcut(QKeySequence("e"));
    toggleEnergyAction->setCheckable(true);
    toggleEnergyAction->setChecked(this->settings->is_energy_displayed);
    connect(toggleEnergyAction, &QAction::triggered, this, &MainWindow::toggleEnergy);

    auto resetHistoryAction = new QAction("Reset History", this);
    resetHistoryAction->setShortcut(QKeySequence("r"));
    connect(resetHistoryAction, &QAction::triggered, this, &MainWindow::resetMeasurementHistory);

    auto setBaseCurrentAction = new QAction("Set base current", this);
    setBaseCurrentAction->setShortcut(QKeySequence("d"));
    connect(setBaseCurrentAction, &QAction::triggered, this, &MainWindow::setBaseCurrent);

    auto resetBaseCurrentAction = new QAction("Reset base current", this);
    resetBaseCurrentAction->setShortcut(QKeySequence("x"));
    connect(resetBaseCurrentAction, &QAction::triggered, this, &MainWindow::resetBaseCurrent);

    // Menu bar
    auto *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Settings", this, &MainWindow::showSettings);
    fileMenu->addSeparator();
    fileMenu->addAction("&Change Device", this, &MainWindow::showDeviceSelectionDialog);
    fileMenu->addAction(toggleEnergyAction);
    fileMenu->addAction(setBaseCurrentAction);
    fileMenu->addAction(resetBaseCurrentAction);
    fileMenu->addAction(resetHistoryAction);

    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    //    auto *viewMenu = menuBar()->addMenu("&View");
    //    viewMenu->addAction("Toggle &OSD", this, &MainWindow::toggleOSD);
    positionWidgets();
}

void MainWindow::positionWidgets() {
    if (!centralWidget())
        return;

    const int windowWidth = centralWidget()->width();
    const int windowHeight = centralWidget()->height();
    const int margin = 5;
    const int labelSpacing = 0;

    // Calculate positions based on window size
    // Top row - voltage and current side by side
    const int topY = margin;
    const int leftColumnX = margin;
    const int rightColumnX = windowWidth / 2;
    const int lineSpacing = -10;

    const int smallWidth =
            windowWidth / 4 -
            margin; // lblPower->fontMetrics().averageCharWidth() * 8;
    const int smallHeight = lblPower->fontMetrics().height() + 10;
    const int smallWideWidth =
            windowWidth / 2 -
            margin; // lblPower->fontMetrics().averageCharWidth() * 16;

    // Position voltage label (top left)
    lblVoltage->move(leftColumnX, topY);
    lblVoltage->resize(windowWidth,
                       lblVoltage->fontMetrics().height() + 10);

    // Position current label (top right)
    lblCurrent->move(leftColumnX, topY);
    lblCurrent->resize(windowWidth - margin * 2,
                       lblCurrent->fontMetrics().height() + 10);

    // Second row - power, energy, min/max current
    const int secondRowY =
            topY + lblVoltage->height() + labelSpacing + lineSpacing;

    lblPower->move(leftColumnX, secondRowY);
    lblPower->resize(smallWidth * 2, smallHeight);

    lblEnergy->move(leftColumnX + smallWidth / 2, secondRowY);
    lblEnergy->resize(smallWidth * 2, smallHeight);

    lblMinMaxCurrent->move(leftColumnX, secondRowY);
    lblMinMaxCurrent->resize(windowWidth - margin * 2, smallHeight);

    // Position CurrentGraph widget at the bottom
    const int graphY = secondRowY + smallHeight + labelSpacing;
    const int graphHeight = windowHeight - graphY - margin;

    if (graphHeight > 100) {
        // Only show graph if there's enough space
        m_currentGraph->move(margin, graphY);
        m_currentGraph->resize(windowWidth - margin * 2, graphHeight);
        m_currentGraph->show();
    } else {
        m_currentGraph->hide();
    }
}

// Override resizeEvent to reposition widgets when window is resized
void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    positionWidgets();
    this->settings->window_left = this->pos().x();
    this->settings->window_top = this->pos().y();
    this->settings->window_width = this->width();
    this->settings->window_height = this->height();
    this->settings->saveSettings();
}

void MainWindow::onPowerDataReceived(const PowerData &data) {
    this->lastDataRaw = data;
    static bool lastWasInvalid = false;
    auto norm_data = this->normalize(data);
    if (norm_data.current < settings->min_current || norm_data.voltage < 2.0) {
        if (!lastWasInvalid) {
            this->m_history->push(norm_data);
            lastWasInvalid = true;
        }
    } else {
        lastWasInvalid = false;
        this->m_history->push(norm_data);
    }
}

void MainWindow::onDeviceConnected(const QString &deviceName) {
    showStatusMessage("Connected to " + deviceName);
    m_updateTimer->start();
}

void MainWindow::onDeviceDisconnected() {
    showStatusMessage("Device disconnected");
    m_updateTimer->stop();
    updateUINoData();
}

void MainWindow::showSettings() { m_settingsdialog->show(); }

void MainWindow::updateLabels() {
    double maxVoltage;
    double maxCurrent;
    double maxPower;
    double totalMinCurrent;
    double totalMaxCurrent;
    if (this->m_history->is_empty()) {
        this->updateUINoData();
        return;
    }
    auto last = this->m_history->atByAge(0);
    // if (last.voltage < 1.0 || last.current < this->settings->min_current) {
    //     this->updateUINoData();
    //     return;
    // }
    this->m_history->minMaxCurrentLastN(this->m_history->size(), totalMinCurrent,
                                        totalMaxCurrent);
    if (!this->m_history->maxValuesLastN(3, maxVoltage, maxCurrent, maxPower)) {
        maxVoltage = this->lastDataRaw.voltage;
        maxCurrent = this->lastDataRaw.current;
        maxPower = maxCurrent * maxVoltage;
    }
        lblVoltage->setText(QString("%1V").arg(maxVoltage, 0, 'f', 2));
        lblCurrent->setText(QString("%1A").arg(maxCurrent, 0, 'f', 4));
        lblPower->setText(QString("%1W").arg(maxPower, 0, 'f', 3));
        lblEnergy->setText(QString("%1Wh").arg(last.energy, 0, 'f', 3));
        lblMinMaxCurrent->setText(QString("%1-%2A")
            .arg(totalMinCurrent, 0, 'f', 3)
            .arg(totalMaxCurrent, 0, 'f', 3));
}

void MainWindow::updateUINoData() {
    double totalMinCurrent;
    double totalMaxCurrent;
    this->m_history->minMaxCurrentLastN(this->m_history->size(), totalMinCurrent,
                                        totalMaxCurrent);
    lblVoltage->setText(QString("---"));
    lblCurrent->setText(QString("---"));
    lblPower->setText(QString("---"));
    lblEnergy->setText(QString("---"));
    lblMinMaxCurrent->setText(QString("%1 - %2A")
        .arg(totalMinCurrent, 0, 'f', 3)
        .arg(totalMaxCurrent, 0, 'f', 3));
}

void MainWindow::setBackgroundColor(const QColor &color) {
    // Only set background for the main window itself, not children
    setStyleSheet(
        QString("MainWindow { background-color: %1; }").arg(color.name()));
}

void MainWindow::resetMeasurementHistory() {
    if (m_history) {
        m_history->reset();
        showStatusMessage("Measurement history reset", 3000);

        // Update labels immediately to reflect the reset
        updateLabels();
    }
}

void MainWindow::setBaseCurrent() {
    if (!m_history || this->m_history->is_empty()) {
        return;
    }
    auto last = this->m_history->atByAge(0);
    this->settings->current_diff_ma = last.current * 1000.0f + this->settings->current_diff_ma;
    showStatusMessage("Base current set to " + QString::number(last.current) + "A", 3000);
}

void MainWindow::resetBaseCurrent() {
    this->settings->current_diff_ma = 0;
    showStatusMessage("Base current reset", 3000);
}

void MainWindow::onPrimaryFontChanged(const QFont &font) {
    qDebug() << "Primary Font changed: " << font.family() << font.pointSize();
    this->fntPrimary.setFamily(font.family());
    this->fntPrimary.setPointSize(font.pointSize());
    this->lblVoltage->setFont(font);
    this->lblCurrent->setFont(font);
    this->repaint();
}

void MainWindow::onSecondaryFontChanged(const QFont &font) {
    qDebug() << "Secondary Font changed: " << font.family() << font.pointSize();
    this->fntSecondary.setFamily(font.family());
    this->fntSecondary.setPointSize(font.pointSize());
    this->lblEnergy->setFont(this->fntSecondary);
    this->lblMinMaxCurrent->setFont(this->fntSecondary);
    this->lblPower->setFont(this->fntSecondary);
    this->repaint();
}

void MainWindow::onColorChanged() {
    this->lblVoltage->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");
    this->lblCurrent->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");
    this->lblPower->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");
    this->lblEnergy->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");
    this->lblMinMaxCurrent->setStyleSheet(
        "QLabel { color: " + settings->color_text.name() + "; }");
    this->setBackgroundColor(settings->color_bg);
}

PowerData MainWindow::normalize(const PowerData &data) {
    if (this->settings->current_diff_ma == 0) return data;
    PowerData normalized = data;
    normalized.current = data.current - (static_cast<float>(this->settings->current_diff_ma) / 1000.0f);
    if (normalized.current < 0) normalized.current = 0;
    return normalized;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == lblMinMaxCurrent && event->type() == QEvent::MouseButtonDblClick) {
        auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            resetMeasurementHistory();
            return true; // Event handled
        }
    }

    // Pass the event to the base class
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::showAboutDialog() {
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}
