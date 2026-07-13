#include "MainWindow.h"

#include "AboutDialog.h"
#include "DeviceSelectionDialog.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <QDebug>
#include <cmath>

MainWindow::MainWindow(OsdSettings *settings,
                       QWidget *parent) // NOLINT(*-pro-type-member-init)
    : QMainWindow(parent), settings(settings),
      m_powerMonitor(new PowerMonitor(this)),
      m_deviceManager(new DeviceManager(this)),
      m_settingsdialog(new SettingsDialog(settings, this)),
      m_history(new MeasurementHistory(1000)), // todo change hard coded value
      m_pipeline(new MeasurementPipeline(this)),
      m_updateTimer(new QTimer(this)), m_statusBarHideTimer(new QTimer(this)),
      m_deviceSelectionDialog(nullptr) {
    this->m_currentGraph = new CurrentGraph(this, m_pipeline, settings);
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
    connect(m_deviceManager, &DeviceManager::btDiscoveryStatusChanged, this,
            &MainWindow::onBtDiscoveryStatusChanged);

    // Setup timers
    m_updateTimer->setInterval(33); // 30 fps fixed render loop
    m_updateTimer->setTimerType(Qt::PreciseTimer);
    connect(m_updateTimer, &QTimer::timeout, [this] {
        this->updateLabels();
        this->m_currentGraph->refresh();
    });
    m_statusBarHideTimer->setSingleShot(true); // Only fire once
    connect(m_statusBarHideTimer, &QTimer::timeout, this,
            &MainWindow::hideStatusBar);

    m_pipeline->setMinCurrentThreshold(settings->min_current);
    m_pipeline->setPausedThresholdMs(1000);
    connect(m_pipeline, &MeasurementPipeline::pausedChanged, m_currentGraph,
            &CurrentGraph::refresh);

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
        }
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
    m_deviceManager->stopBtScanning();

    if (!m_deviceSelectionDialog) {
        m_deviceSelectionDialog = new DeviceSelectionDialog(this);
    }

    if (m_deviceSelectionDialog->exec() == QDialog::Accepted) {
        qDebug() << "Accepted DeviceSelectionDialog";
        auto connectionType = m_deviceSelectionDialog->getSelectedConnectionType();

        qDebug() << "Selected connection type: " << static_cast<int>(connectionType);
        if (connectionType ==
            DeviceSelectionDialog::ConnectionType::BluetoothAuto) {
            statusBar()->showMessage("Scanning for Bluetooth devices...");
            m_deviceManager->startBtScanning();
            this->settings->last_device = "ble";
            this->settings->saveSettings();
        } else if (connectionType ==
                   DeviceSelectionDialog::ConnectionType::SerialPort) {
            m_deviceManager->stopBtScanning();
            QString selectedPort = m_deviceSelectionDialog->getSelectedSerialPort();
            qDebug() << "Selected serial port: " << selectedPort;
            this->settings->last_device = selectedPort;
            this->settings->saveSettings();
            statusBar()->showMessage(QString("Connected to %1").arg(selectedPort));
        }
    } else {
        this->startReconnectTimer();
    }
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
    this->lblEnergy->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
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

    QAction *audioAction = fileMenu->addAction(tr("Audio Output"));
    audioAction->setCheckable(true);
    audioAction->setChecked(settings->is_audio_enabled);
    audioAction->setShortcut(QKeySequence("a"));
    connect(audioAction, &QAction::toggled, this, &MainWindow::toggleAudio);

    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    QAction *logScaleAction = viewMenu->addAction(tr("Logarithmic Graph Time"));
    logScaleAction->setCheckable(true);
    logScaleAction->setChecked(settings->graph_log_scale);
    connect(logScaleAction, &QAction::toggled, this, &MainWindow::toggleGraphLogScale);

    QAction *peaksAction = viewMenu->addAction(tr("Show Graph Peaks"));
    peaksAction->setCheckable(true);
    peaksAction->setChecked(settings->show_graph_peaks);
    peaksAction->setShortcut(QKeySequence("p"));
    connect(peaksAction, &QAction::toggled, this, &MainWindow::toggleGraphPeaks);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *manualAction = helpMenu->addAction(tr("&User Manual"));
    manualAction->setShortcut(QKeySequence("F1"));
    connect(manualAction, &QAction::triggered, this, &MainWindow::showUserManual);
    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

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
    const int lineSpacing = -10;

    const int smallHeight = lblPower->fontMetrics().height() + 10;

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

    // Allocate horizontal space proportionally to the expected text widths:
    // power ~ 1/4, energy ~ 1/4 (centered between the other two),
    // min/max current ~ 1/2 of the available width.
    const int secondRowWidth = windowWidth - 2 * margin;
    const int colGap = 8;
    const int powerWidth = secondRowWidth / 4;
    const int energyWidth = secondRowWidth / 4;
    const int minmaxWidth = secondRowWidth - powerWidth - energyWidth - 2 * colGap;

    lblPower->move(margin, secondRowY);
    lblPower->resize(powerWidth, smallHeight);

    lblEnergy->move(margin + powerWidth + colGap, secondRowY);
    lblEnergy->resize(energyWidth, smallHeight);

    lblMinMaxCurrent->move(margin + powerWidth + energyWidth + 2 * colGap, secondRowY);
    lblMinMaxCurrent->resize(minmaxWidth, smallHeight);

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

void MainWindow::moveEvent(QMoveEvent *event) {
    QMainWindow::moveEvent(event);
    this->settings->window_left = this->pos().x();
    this->settings->window_top = this->pos().y();
    this->settings->saveSettings();
}

void MainWindow::onPowerDataReceived(const PowerData &data) {
    this->lastDataRaw = data;
    auto norm_data = this->normalize(data);

    // Always feed the pipeline; invalid samples are also useful to show gaps.
    this->m_pipeline->pushSample(norm_data);

    // Keep the raw rolling window available for legacy label stats.
    static bool lastWasInvalid = false;
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
    m_pipeline->reset();
    m_history->reset();
    m_currentGraph->setLive();
    m_updateTimer->start();
}

void MainWindow::onDeviceDisconnected() {
    showStatusMessage("Device disconnected");
    m_updateTimer->stop();
    updateUINoData();
}

void MainWindow::onBtDiscoveryStatusChanged(const QString &message) {
    if (message.isEmpty()) {
        statusBar()->setVisible(false);
        return;
    }
    statusBar()->setVisible(true);
    statusBar()->showMessage(message);
}

void MainWindow::showSettings() {
  m_settingsdialog->show();
  m_pipeline->setMinCurrentThreshold(settings->min_current);
}

void MainWindow::updateLabels() {
    double maxVoltage;
    double maxCurrent;
    double maxPower;
    double totalMinCurrent;
    double totalMaxCurrent;

    const int labelWindow = std::clamp(settings->label_sample_window, 1, 10);

    if (!m_pipeline->maxValuesRawLastN(labelWindow, maxVoltage, maxCurrent, maxPower)) {
        this->updateUINoData();
        return;
    }

    DisplayFrame lastFrame = m_pipeline->latestFrame();

    if (m_currentGraph->hasVisibleData()) {
        totalMinCurrent = m_currentGraph->visibleMinCurrent();
        totalMaxCurrent = m_currentGraph->visibleMaxCurrent();
    } else {
        totalMinCurrent = 0.0;
        totalMaxCurrent = 0.0;
    }

    lblVoltage->setText(QString("%1V").arg(maxVoltage, 0, 'f', 2));
    lblCurrent->setText(QString("%1A").arg(maxCurrent, 0, 'f', 4));
    // Fixed-width format: up to 3 integer digits + 2 decimals (e.g. " 100.00W")
    // so the label width stays constant around the 100 W transition.
    lblPower->setText(QString("%1W").arg(maxPower, 6, 'f', 2, ' '));
    lblEnergy->setText(QString("%1Wh").arg(lastFrame.energyWh, 0, 'f', 3));
    lblMinMaxCurrent->setText(QString("%1-%2A")
        .arg(totalMinCurrent, 0, 'f', 3)
        .arg(totalMaxCurrent, 0, 'f', 3));

    // Audio is driven by the time-normalized 30 Hz frame path.
    if (settings->is_audio_enabled && m_audioGenerator) {
        double currentA = std::max(lastFrame.current, 0.0);
        double normalizedCurrent = std::min(currentA / 5.0, 1.0);
        double freq = 400.0 + (4000.0 - 400.0) * std::sqrt(normalizedCurrent);
        m_audioGenerator->setFrequency(freq);

        double stddev10 = m_pipeline->stdDevCurrentLastN(10);
        double stddev3 = m_pipeline->stdDevCurrentLastN(3);
        double diff = std::abs(stddev3 - stddev10);
        double amp = std::min(diff / 0.05, 1.0) * 0.1;

        // Silence audio when there is no real current.
        if (currentA < settings->min_current || lastFrame.sampleCount == 0) {
            amp = 0.0;
        }
        m_audioGenerator->setAmplitude(amp);
    }

}

void MainWindow::updateUINoData() {
    if (m_audioGenerator) {
        m_audioGenerator->setAmplitude(0.0);
    }
    double totalMinCurrent = 0.0;
    double totalMaxCurrent = 0.0;
    if (m_currentGraph->hasVisibleData()) {
        totalMinCurrent = m_currentGraph->visibleMinCurrent();
        totalMaxCurrent = m_currentGraph->visibleMaxCurrent();
    }
    lblVoltage->setText(QString("---"));
    lblCurrent->setText(QString("---"));
    lblPower->setText(QString("---"));
    lblEnergy->setText(QString("---"));
    lblMinMaxCurrent->setText(QString("%1-%2A")
        .arg(totalMinCurrent, 0, 'f', 3)
        .arg(totalMaxCurrent, 0, 'f', 3));
}

void MainWindow::setBackgroundColor(const QColor &color) {
    // Only set background for the main window itself, not children
    setStyleSheet(
        QString("MainWindow { background-color: %1; }").arg(color.name()) +
        QString("QStatusBar { color: %1; background-color: %2; }")
            .arg(settings->color_text.name())
            .arg(color.name()));
}

void MainWindow::resetMeasurementHistory() {
    if (m_history) {
        m_history->reset();
    }
    if (m_pipeline) {
        m_pipeline->reset();
    }
    if (m_currentGraph) {
        m_currentGraph->setLive();
    }
    showStatusMessage("Measurement history reset", 3000);

    // Update labels immediately to reflect the reset
    updateLabels();
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

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // Forward cursor keys and Home to the graph for history navigation,
    // unless a dialog or text field has focus.
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_L:
        m_currentGraph->setFocus();
        m_currentGraph->handleKey(event);
        if (event->isAccepted()) {
            return;
        }
        break;
    default:
        break;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::showAboutDialog() {
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void MainWindow::showUserManual() {
    const QString manualFileName = "USER_MANUAL.html";
    QStringList candidates;

    const QString appDir = QCoreApplication::applicationDirPath();

#ifdef Q_OS_MACOS
    candidates << QDir(appDir).filePath("../Resources/" + manualFileName);
    candidates << QDir(appDir).filePath(manualFileName);
#elif defined(Q_OS_WIN)
    candidates << QDir(appDir).filePath(manualFileName);
#else
    candidates << "/usr/share/doc/usb-power-osd/" + manualFileName;
    candidates << "/usr/local/share/doc/usb-power-osd/" + manualFileName;
    candidates << QDir(appDir).filePath("../share/doc/usb-power-osd/" + manualFileName);
    candidates << QDir(appDir).filePath(manualFileName);
#endif

    for (const QString &path : candidates) {
        QFileInfo info(path);
        if (info.exists() && info.isFile()) {
            QUrl url = QUrl::fromLocalFile(info.absoluteFilePath());
            if (QDesktopServices::openUrl(url)) {
                return;
            }
        }
    }

    QMessageBox::warning(this, tr("User Manual"),
                         tr("Could not find the user manual (%1).\n"
                            "Please visit the project page for documentation.")
                             .arg(manualFileName));
}

void MainWindow::toggleGraphLogScale() {
    settings->graph_log_scale = !settings->graph_log_scale;
    settings->saveSettings();
    m_currentGraph->invalidateCache();
    m_currentGraph->update();
}

void MainWindow::toggleGraphPeaks() {
    settings->show_graph_peaks = !settings->show_graph_peaks;
    settings->saveSettings();
    m_currentGraph->update();
}

void MainWindow::toggleAudio() {
    settings->is_audio_enabled = !settings->is_audio_enabled;
    settings->saveSettings();

    if (settings->is_audio_enabled) {
        if (!m_audioSink) {
            QAudioFormat format;
            format.setSampleRate(44100);
            format.setChannelCount(1);
            format.setSampleFormat(QAudioFormat::Float);

            m_audioGenerator = new AudioGenerator(format, this);
            m_audioSink = new QAudioSink(format, this);
        }
        m_audioGenerator->start();
        m_audioSink->start(m_audioGenerator);
    } else {
        if (m_audioSink) {
            m_audioSink->stop();
            m_audioGenerator->stop();
        }
    }
}
