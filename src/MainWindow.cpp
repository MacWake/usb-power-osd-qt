#include "MainWindow.h"
#include "DeviceSelectionDialog.h"
#include <QApplication>
#include <QCloseEvent>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QTimer>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) // NOLINT(*-pro-type-member-init)
    : QMainWindow(parent), m_powerMonitor(new PowerMonitor(this)),
      m_deviceManager(new DeviceManager(this)),
      m_settings(new SettingsDialog(this)),
      m_history(new MeasurementHistory(1000)), // todo change hard coded value
      m_updateTimer(new QTimer(this)),
      m_statusBarHideTimer(new QTimer(this)),
      m_deviceSelectionDialog(nullptr)

{
  if (!this->settings) {
    this->settings = new OsdSettings("MacWake", "USB Display", this);
    this->settings->init();
    this->m_currentGraph = new CurrentGraph(this, m_history, settings);
  }
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

  // Check if last device is available, show selection dialog if not
  if (!DeviceSelectionDialog::isLastDeviceAvailable()) {
    // Use a timer to show the dialog after the main window is fully initialized
    QTimer::singleShot(100, this, &MainWindow::connectLastDevice);
  } else {
    // Start scanning with last known device settings
    m_deviceManager->startScanning();
    statusBar()->showMessage("Scanning for USB Power devices...");
  }
}

MainWindow::~MainWindow() = default;

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

void MainWindow::connectLastDevice()
{
  if (!settings->last_device.isEmpty()) {
    this->m_deviceManager->tryConnect(settings->last_device);
  }
  if (!m_deviceSelectionDialog) {
    m_deviceSelectionDialog = new DeviceSelectionDialog(this);
  }
  
  if (m_deviceSelectionDialog->exec() == QDialog::Accepted) {
    auto connectionType = m_deviceSelectionDialog->getSelectedConnectionType();
    
    if (connectionType == DeviceSelectionDialog::ConnectionType::BluetoothAuto) {
      statusBar()->showMessage("Scanning for Bluetooth devices...");
      m_deviceManager->startScanning();
    } else if (connectionType == DeviceSelectionDialog::ConnectionType::SerialPort) {
      QString selectedPort = m_deviceSelectionDialog->getSelectedSerialPort();
      statusBar()->showMessage(QString("Connecting to %1...").arg(selectedPort));
      m_deviceManager->startScanning();
    }
  } else {
    // User cancelled - exit application
    statusBar()->showMessage("No device selected. Exiting...");
    QTimer::singleShot(2000, QApplication::instance(), &QApplication::quit);
  }
}

void MainWindow::showDeviceSelectionDialog()
{
  if (!m_deviceSelectionDialog) {
    m_deviceSelectionDialog = new DeviceSelectionDialog(this);
  }

  if (m_deviceSelectionDialog->exec() == QDialog::Accepted) {
    auto connectionType = m_deviceSelectionDialog->getSelectedConnectionType();

    if (connectionType == DeviceSelectionDialog::ConnectionType::BluetoothAuto) {
      statusBar()->showMessage("Scanning for Bluetooth devices...");
      m_deviceManager->startScanning();
    } else if (connectionType == DeviceSelectionDialog::ConnectionType::SerialPort) {
      QString selectedPort = m_deviceSelectionDialog->getSelectedSerialPort();
      statusBar()->showMessage(QString("Connecting to %1...").arg(selectedPort));
      m_deviceManager->startScanning();
    }
  } else {
    // User cancelled - exit application
    statusBar()->showMessage("No device selected. Exiting...");
    QTimer::singleShot(2000, QApplication::instance(), &QApplication::quit);
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
  this->fntVoltage = new QFont(this->settings->measurements_font,
                               this->settings->volts_font_size);
  this->fntCurrent = new QFont(this->settings->measurements_font,
                               this->settings->amps_font_size);
  this->fntPower = new QFont(this->settings->measurements_font,
                             this->settings->power_font_size);
  this->fntEnergy = new QFont(this->settings->measurements_font,
                              this->settings->energy_font_size);
  this->lblVoltage->setFont(*fntVoltage);
  this->lblCurrent->setFont(*fntCurrent);
  this->lblPower->setFont(*fntPower);
  this->lblEnergy->setFont(*fntEnergy);
  this->lblMinMaxCurrent->setFont(*fntPower);

  QColor color(255 - settings->color_bg.red(),
                       255 - settings->color_bg.green(),
                       255 - settings->color_bg.blue());
  this->statusBar()->setStyleSheet("color: " + color.name() + ";");
  this->lblVoltage->setStyleSheet("QLabel { color: " + color.name() + "; }");
  this->lblCurrent->setStyleSheet("QLabel { color: " + color.name() + "; }");
  this->lblPower->setStyleSheet("QLabel { color: " + color.name() + "; }");
  this->lblEnergy->setStyleSheet("QLabel { color: " + color.name() + "; }");
  this->lblMinMaxCurrent->setStyleSheet( "QLabel { color: " + color.name() + "; }");

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
  lblMinMaxCurrent->setParent(centralWidget);
  m_currentGraph->setParent(centralWidget);

  // Menu bar
  auto *fileMenu = menuBar()->addMenu("&File");
  fileMenu->addAction("&Settings", this, &MainWindow::showSettings);
  fileMenu->addSeparator();
  fileMenu->addAction("&Change Device", this, &MainWindow::showDeviceSelectionDialog);
  fileMenu->addSeparator();
  fileMenu->addAction("E&xit", this, &QWidget::close);

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

  const int smallWidth = lblPower->fontMetrics().averageCharWidth() * 8;
  const int smallHeight = lblPower->fontMetrics().height() + 10;
  const int smallWideWidth = lblPower->fontMetrics().averageCharWidth() * 16;

  // Position voltage label (top left)
  lblVoltage->move(leftColumnX, topY);
  lblVoltage->resize(windowWidth / 2 - margin,
                     lblVoltage->fontMetrics().height() + 10);

  // Position current label (top right)
  lblCurrent->move(rightColumnX, topY);
  lblCurrent->resize(windowWidth / 2 - margin,
                     lblCurrent->fontMetrics().height() + 10);

  // Second row - power, energy, min/max current
  const int secondRowY = topY + lblVoltage->height() + labelSpacing + lineSpacing;

  lblPower->move(leftColumnX, secondRowY);
  lblPower->resize(smallWidth, smallHeight);

  lblEnergy->move((windowWidth - smallWidth) / 2, secondRowY);
  lblEnergy->resize(smallWidth, smallHeight);

  lblMinMaxCurrent->move(windowWidth - smallWideWidth - margin, secondRowY);
  lblMinMaxCurrent->resize(smallWideWidth, smallHeight);

  // Position CurrentGraph widget at the bottom
  const int graphY = secondRowY + smallHeight + labelSpacing;
  const int graphHeight = windowHeight - graphY - margin;

  if (graphHeight > 100) { // Only show graph if there's enough space
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
}

void MainWindow::onPowerDataReceived(const PowerData &data) {
  if (data.current > 0.001 && data.voltage > 4.0)
    this->m_history->push(data);
}

void MainWindow::onDeviceConnected(const QString &deviceName) {
  showStatusMessage("Connected to " + deviceName + "; starting timer");
  m_updateTimer->start();
}

void MainWindow::onDeviceDisconnected() {
  showStatusMessage("Device disconnected");
  m_updateTimer->stop();
  updateUINoData();
}

void MainWindow::showSettings() { m_settings->show(); }

void MainWindow::updateLabels() {

  double maxVoltage;
  double maxCurrent;
  double maxPower;
  double totalMinCurrent;
  double totalMaxCurrent;
  auto last = this->m_history->atByAge(0);
  if (last.voltage < 1.0 || last.current < this->settings->min_current) {
    this->updateUINoData();
    return;
  }
  this->m_history->minMaxCurrentLastN(this->m_history->size(), totalMinCurrent,
                                      totalMaxCurrent);
  if (this->m_history->maxValuesLastN(3, maxVoltage, maxCurrent, maxPower)) {
    lblVoltage->setText(QString("%1V").arg(maxVoltage, 0, 'f', 2));
    lblCurrent->setText(QString("%1A").arg(maxCurrent, 0, 'f', 3));
    lblPower->setText(QString("%1W").arg(maxPower, 0, 'f', 3));
    lblEnergy->setText(QString("%1Wh").arg(last.energy, 0, 'f', 3));
    lblMinMaxCurrent->setText(QString("%1 - %2A")
                                  .arg(totalMinCurrent, 0, 'f', 3)
                                  .arg(totalMaxCurrent, 0, 'f', 3));
  }
}
void MainWindow::updateUINoData() {
  double totalMinCurrent;
  double totalMaxCurrent;
  auto last = this->m_history->atByAge(0);
  this->m_history->minMaxCurrentLastN(this->m_history->size(), totalMinCurrent,
                                      totalMaxCurrent);
  lblVoltage->setText(QString("---"));
  lblCurrent->setText(QString("---"));
  lblPower->setText(QString("---"));
  lblEnergy->setText(QString("%1 Wh").arg(last.energy, 0, 'f', 3));
  lblMinMaxCurrent->setText(QString("%1 - %2A")
                                .arg(totalMinCurrent, 0, 'f', 3)
                                .arg(totalMaxCurrent, 0, 'f', 3));
}

void MainWindow::setBackgroundColor(const QColor &color) {
  setStyleSheet(QString("* { background-color: %1; }"
            "QToolTip { background-color: %2; color: %3; }"
).arg(color.name(), color.name()).arg((color.lightness() > 128) ? Qt::black : Qt::white
));
}

void MainWindow::resetMeasurementHistory() {
  if (m_history) {
    m_history->reset();
    showStatusMessage("Measurement history reset", 3000);

    // Update labels immediately to reflect the reset
    updateLabels();
  }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  if (obj == lblMinMaxCurrent && event->type() == QEvent::MouseButtonDblClick) {
    auto mouseEvent = dynamic_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      resetMeasurementHistory();
      return true; // Event handled
    }
  }

  // Pass the event to the base class
  return QMainWindow::eventFilter(obj, event);
}
