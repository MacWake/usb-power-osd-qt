#include "MainWindow.h"
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QGroupBox>
#include <QStatusBar>
#include <QMenuBar>
#include <QApplication>
#include <QCloseEvent>
#include <QMessageBox>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_powerMonitor(new PowerMonitor(this))
    , m_deviceManager(new DeviceManager(this))
    , m_settings(new SettingsDialog(this))
    , m_updateTimer(new QTimer(this))
{
    if (!MainWindow::settings) {
        MainWindow::settings = new OsdSettings("MacWake", "Owon1041", this);
        MainWindow::settings->init();
    }

    if (MainWindow::settings->window_width > 0 &&
        MainWindow::settings->window_height > 0) {
        setGeometry(MainWindow::settings->window_left,
                    MainWindow::settings->window_top,
                    MainWindow::settings->window_width,
                    MainWindow::settings->window_height);
        }

    setupUI();

    // Connect signals
    connect(m_deviceManager, &DeviceManager::powerDataReceived,
        this, &MainWindow::onPowerDataReceived);
    connect(m_deviceManager, &DeviceManager::deviceConnected,
            this, &MainWindow::onDeviceConnected);
    connect(m_deviceManager, &DeviceManager::deviceDisconnected,
            this, &MainWindow::onDeviceDisconnected);
    
    // // Setup update timer
    // m_updateTimer->setInterval(1000); // 1 second
    // connect(m_updateTimer, &QTimer::timeout, [this]() {
    //     m_deviceManager->requestPowerData();
    // });
    //
    // Start scanning for devices
    m_deviceManager->startScanning();
    
    statusBar()->showMessage("Scanning for USB Power devices...");
}

MainWindow::~MainWindow()
{
}

// ReSharper disable CppDFAMemoryLeak
void MainWindow::setupUI()
{
    setWindowTitle("USB Power OSD");
    setMinimumSize(400, 300);
    
    auto *centralWidget = new QWidget;
    setCentralWidget(centralWidget);
    
    auto *mainLayout = new QVBoxLayout(centralWidget);
    
    // Device status group
    auto *deviceGroup = new QGroupBox("Device Status");
    auto *deviceLayout = new QHBoxLayout(deviceGroup);
    
    m_statusLabel = new QLabel("Disconnected");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    deviceLayout->addWidget(new QLabel("Status:"));
    deviceLayout->addWidget(m_statusLabel);
    deviceLayout->addStretch();
    
    mainLayout->addWidget(deviceGroup);
    
    // Power data group
    auto *powerGroup = new QGroupBox("Power Data");
    auto *powerLayout = new QGridLayout(powerGroup);
    
    // Voltage
    powerLayout->addWidget(new QLabel("Voltage:"), 0, 0);
    m_voltageLabel = new QLabel("0.00 V");
    m_voltageLabel->setStyleSheet("QLabel { font-weight: bold; }");
    powerLayout->addWidget(m_voltageLabel, 0, 1);
    m_voltageBar = new QProgressBar();
    m_voltageBar->setRange(0, 2500); // 0-25V in centivolts
    powerLayout->addWidget(m_voltageBar, 0, 2);
    
    // Current
    powerLayout->addWidget(new QLabel("Current:"), 1, 0);
    m_currentLabel = new QLabel("0.00 A");
    m_currentLabel->setStyleSheet("QLabel { font-weight: bold; }");
    powerLayout->addWidget(m_currentLabel, 1, 1);
    m_currentBar = new QProgressBar();
    m_currentBar->setRange(0, 500); // 0-5A in centiamps
    powerLayout->addWidget(m_currentBar, 1, 2);
    
    // Power
    powerLayout->addWidget(new QLabel("Power:"), 2, 0);
    m_powerLabel = new QLabel("0.00 W");
    m_powerLabel->setStyleSheet("QLabel { font-weight: bold; }");
    powerLayout->addWidget(m_powerLabel, 2, 1);
    
    // Energy
    powerLayout->addWidget(new QLabel("Energy:"), 3, 0);
    m_energyLabel = new QLabel("0.00 Wh");
    m_energyLabel->setStyleSheet("QLabel { font-weight: bold; }");
    powerLayout->addWidget(m_energyLabel, 3, 1);
    
    mainLayout->addWidget(powerGroup);
    
    // Control buttons
    auto *buttonLayout = new QHBoxLayout();
    auto *osdButton = new QPushButton("Toggle OSD");
    auto *settingsButton = new QPushButton("Settings");
    
    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::showSettings);
    
    buttonLayout->addWidget(osdButton);
    buttonLayout->addWidget(settingsButton);
    buttonLayout->addStretch();
    
    mainLayout->addLayout(buttonLayout);
    mainLayout->addStretch();
    
    // Menu bar
    auto *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Settings", this, &MainWindow::showSettings);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);
    
    auto *viewMenu = menuBar()->addMenu("&View");
//    viewMenu->addAction("Toggle &OSD", this, &MainWindow::toggleOSD);
}

void MainWindow::onPowerDataReceived(const PowerData &data)
{
    updateUI(data);
}

void MainWindow::onDeviceConnected(const QString &deviceName)
{
    m_statusLabel->setText("Connected: " + deviceName);
    m_statusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    statusBar()->showMessage("Connected to " + deviceName);
    
    m_updateTimer->start();
}

void MainWindow::onDeviceDisconnected()
{
    m_statusLabel->setText("Disconnected");
    m_statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusBar()->showMessage("Device disconnected");
    
    m_updateTimer->stop();
    
    // Clear data display
    PowerData emptyData;
    updateUI(emptyData);
}

void MainWindow::showSettings()
{
    m_settings->show();
}

void MainWindow::updateUI(const PowerData &data)
{
    m_voltageLabel->setText(QString("%1 V").arg(data.voltage, 0, 'f', 2));
    m_currentLabel->setText(QString("%1 A").arg(data.current, 0, 'f', 3));
    m_powerLabel->setText(QString("%1 W").arg(data.power, 0, 'f', 2));
    m_energyLabel->setText(QString("%1 Wh").arg(data.energy, 0, 'f', 2));
    
    m_voltageBar->setValue(static_cast<int>(data.voltage * 100));
    m_currentBar->setValue(static_cast<int>(data.current * 100));
}
