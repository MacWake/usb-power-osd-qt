#include "DeviceSelectionDialog.h"

#include "MainWindow.h"

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSettings>
#include <QStyle>

DeviceSelectionDialog::DeviceSelectionDialog(QWidget *parent)
    : QDialog(parent)
    , m_selectedType(ConnectionType::None)
{
    m_parent = static_cast<MainWindow *>(parent);
    setWindowTitle("Select Connection Method");
    setModal(true);
    setFixedSize(450, 280);
    
    setupUI();
    refreshSerialPorts();
    updateControlStates();
    
    // Load last used device settings
    QString lastDevice = getLastUsedDevice();
    if (lastDevice.startsWith("bluetooth")) {
        m_bluetoothRadio->setChecked(true);
        m_selectedType = ConnectionType::BluetoothAuto;
    } else if (lastDevice.startsWith("serial:")) {
        m_serialRadio->setChecked(true);
        m_selectedType = ConnectionType::SerialPort;
        QString portName = lastDevice.mid(7); // Remove "serial:" prefix
        int index = m_serialPortCombo->findData(portName);
        if (index >= 0) {
            m_serialPortCombo->setCurrentIndex(index);
        }
    } else {
        // Default to Bluetooth if no previous setting
        m_bluetoothRadio->setChecked(true);
        m_selectedType = ConnectionType::BluetoothAuto;
    }
    
    updateControlStates();
}

void DeviceSelectionDialog::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(15);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Title
    m_titleLabel = new QLabel("Choose Device Connection Method");
    m_titleLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; }");
    m_mainLayout->addWidget(m_titleLabel);
    
    // Instruction text
    m_instructionLabel = new QLabel("Select how you want to connect to your USB Power OSD device:");
    m_instructionLabel->setWordWrap(true);
    m_instructionLabel->setStyleSheet("QLabel { color: #666; }");
    m_mainLayout->addWidget(m_instructionLabel);
    
    // Content area
    m_contentLayout = new QGridLayout;
    m_contentLayout->setVerticalSpacing(12);
    m_contentLayout->setHorizontalSpacing(10);
    
    // Connection type radio buttons
    m_connectionTypeGroup = new QButtonGroup(this);
    
    m_bluetoothRadio = new QRadioButton("Bluetooth (Automatic Discovery)");
    m_bluetoothRadio->setToolTip("Automatically discover and connect to USB Power OSD Bluetooth devices");
    m_connectionTypeGroup->addButton(m_bluetoothRadio, static_cast<int>(ConnectionType::BluetoothAuto));
    m_contentLayout->addWidget(m_bluetoothRadio, 0, 0, 1, 3);
    
    m_serialRadio = new QRadioButton("Serial Port (Manual Selection)");
    m_serialRadio->setToolTip("Select a specific serial port to connect to");
    m_connectionTypeGroup->addButton(m_serialRadio, static_cast<int>(ConnectionType::SerialPort));
    m_contentLayout->addWidget(m_serialRadio, 1, 0, 1, 3);
    
    // Serial port selection
    m_serialPortLabel = new QLabel("Serial Port:");
    m_serialPortLabel->setIndent(20);
    m_contentLayout->addWidget(m_serialPortLabel, 2, 0);
    
    m_serialPortCombo = new QComboBox;
    m_serialPortCombo->setMinimumWidth(200);
    m_serialPortCombo->setToolTip("Available serial ports on this system");
    m_contentLayout->addWidget(m_serialPortCombo, 2, 1);
    
    m_refreshSerialButton = new QPushButton("Refresh");
    m_refreshSerialButton->setMaximumWidth(80);
    m_refreshSerialButton->setToolTip("Refresh the list of available serial ports");
    m_contentLayout->addWidget(m_refreshSerialButton, 2, 2);
    
    m_mainLayout->addLayout(m_contentLayout);
    
    // Add some spacing
    m_mainLayout->addStretch();
    
    // Buttons
    m_buttonLayout = new QHBoxLayout;
    m_buttonLayout->addStretch();
    
    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setMinimumWidth(80);
    m_buttonLayout->addWidget(m_cancelButton);
    
    m_okButton = new QPushButton("Connect");
    m_okButton->setMinimumWidth(80);
    m_okButton->setDefault(true);
    m_buttonLayout->addWidget(m_okButton);
    
    m_mainLayout->addLayout(m_buttonLayout);
    
    // Connect signals
    connect(m_bluetoothRadio, &QRadioButton::clicked,
            this, &DeviceSelectionDialog::onConnectionTypeChanged);
    connect(m_serialRadio, &QRadioButton::clicked,
            this, &DeviceSelectionDialog::onConnectionTypeChanged);
    connect(m_refreshSerialButton, &QPushButton::clicked,
            this, &DeviceSelectionDialog::onRefreshSerialPorts);
    connect(m_okButton, &QPushButton::clicked,
            this, &DeviceSelectionDialog::onOkButtonClicked);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &DeviceSelectionDialog::onCancelButtonClicked);
}

void DeviceSelectionDialog::refreshSerialPorts()
{
    m_serialPortCombo->clear();
    
    const auto ports = QSerialPortInfo::availablePorts();
    if (ports.isEmpty()) {
        m_serialPortCombo->addItem("No serial ports available", QString());
        m_serialPortCombo->setEnabled(false);
    } else {
        for (const auto &port : ports) {
            QString displayText = QString("%1 (%2)").arg(port.portName(), port.description());
            m_serialPortCombo->addItem(displayText, port.portName());
        }
        m_serialPortCombo->setEnabled(true);
    }
}

void DeviceSelectionDialog::onConnectionTypeChanged()
{
    if (m_bluetoothRadio->isChecked()) {
        m_selectedType = ConnectionType::BluetoothAuto;
    } else if (m_serialRadio->isChecked()) {
        m_selectedType = ConnectionType::SerialPort;
    }
    updateControlStates();
}

void DeviceSelectionDialog::updateControlStates()
{
    bool isSerialSelected = (m_selectedType == ConnectionType::SerialPort);
    
    m_serialPortLabel->setEnabled(isSerialSelected);
    m_serialPortCombo->setEnabled(isSerialSelected && m_serialPortCombo->count() > 0);
    m_refreshSerialButton->setEnabled(isSerialSelected);
    
    // Enable OK button if a valid selection is made
    bool canConnect = false;
    if (m_selectedType == ConnectionType::BluetoothAuto) {
        canConnect = true;
    } else if (m_selectedType == ConnectionType::SerialPort) {
        canConnect = (m_serialPortCombo->count() > 0 && 
                     !m_serialPortCombo->currentData().toString().isEmpty());
    }
    
    m_okButton->setEnabled(canConnect);
}

void DeviceSelectionDialog::onRefreshSerialPorts()
{
    refreshSerialPorts();
    updateControlStates();
}

void DeviceSelectionDialog::onOkButtonClicked()
{
    if (m_selectedType == ConnectionType::SerialPort) {
        m_selectedSerialPort = m_serialPortCombo->currentData().toString();
        if (m_selectedSerialPort.isEmpty()) {
            QMessageBox::warning(this, "Invalid Selection", 
                                "Please select a valid serial port.");
            return;
        }
    }
    
    saveSelectedDevice();
    accept();
}

void DeviceSelectionDialog::onCancelButtonClicked()
{
    reject();
}

DeviceSelectionDialog::ConnectionType DeviceSelectionDialog::getSelectedConnectionType() const
{
    return m_selectedType;
}

QString DeviceSelectionDialog::getSelectedSerialPort() const
{
    return m_selectedSerialPort;
}

void DeviceSelectionDialog::saveSelectedDevice()
{
    QSettings settings("MacWake", "USB Display");
    
    switch (m_selectedType) {
    case ConnectionType::BluetoothAuto:
        settings.setValue("device/lastUsed", "bluetooth");
        break;
    case ConnectionType::SerialPort:
        settings.setValue("device/lastUsed", QString("serial:%1").arg(m_selectedSerialPort));
        break;
    case ConnectionType::None:
        settings.remove("device/lastUsed");
        break;
    }
    
    settings.sync();
}

QString DeviceSelectionDialog::getLastUsedDevice()
{
    QSettings settings("MacWake", "USB Display");
    return settings.value("device/lastUsed", "").toString();
}
