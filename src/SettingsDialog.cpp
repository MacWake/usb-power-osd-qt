#include "SettingsDialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_settings(new QSettings(this))
{
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI()
{
    setWindowTitle("Settings");
    setModal(true);
    resize(400, 300);
    
    auto *layout = new QVBoxLayout(this);
    
    // OSD Settings
    auto *osdGroup = new QGroupBox("OSD Display");
    auto *osdLayout = new QFormLayout(osdGroup);
    
    m_autoStartOSDCheck = new QCheckBox("Show OSD on startup");
    osdLayout->addRow(m_autoStartOSDCheck);
    
    m_updateIntervalSpin = new QSpinBox();
    m_updateIntervalSpin->setRange(100, 5000);
    m_updateIntervalSpin->setSuffix(" ms");
    m_updateIntervalSpin->setValue(1000);
    osdLayout->addRow("Update interval:", m_updateIntervalSpin);
    
    auto *opacityLayout = new QHBoxLayout();
    m_opacitySlider = new QSlider(Qt::Horizontal);
    m_opacitySlider->setRange(10, 100);
    m_opacitySlider->setValue(90);
    m_opacityLabel = new QLabel("90%");
    opacityLayout->addWidget(m_opacitySlider);
    opacityLayout->addWidget(m_opacityLabel);
    osdLayout->addRow("Opacity:", opacityLayout);
    
    connect(m_opacitySlider, &QSlider::valueChanged,
            this, &SettingsDialog::onOpacityChanged);
    
    layout->addWidget(osdGroup);
    
    // Connection Settings
    auto *connectionGroup = new QGroupBox("Connection");
    auto *connectionLayout = new QFormLayout(connectionGroup);
    
    m_connectionCombo = new QComboBox();
    m_connectionCombo->addItems({"Auto", "Bluetooth Preferred", "Serial Preferred"});
    connectionLayout->addRow("Preference:", m_connectionCombo);
    
    m_notificationsCheck = new QCheckBox("Show connection notifications");
    m_notificationsCheck->setChecked(true);
    connectionLayout->addRow(m_notificationsCheck);
    
    layout->addWidget(connectionGroup);
    
    // Dialog buttons
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccepted);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::onRejected);
    
    layout->addWidget(buttonBox);
}

void SettingsDialog::loadSettings()
{
    m_autoStartOSDCheck->setChecked(m_settings->value("osd/autoStart", false).toBool());
    m_updateIntervalSpin->setValue(m_settings->value("osd/updateInterval", 1000).toInt());
    m_opacitySlider->setValue(m_settings->value("osd/opacity", 90).toInt());
    onOpacityChanged(m_opacitySlider->value());
    
    QString connectionPref = m_settings->value("connection/preference", "Auto").toString();
    m_connectionCombo->setCurrentText(connectionPref);
    
    m_notificationsCheck->setChecked(m_settings->value("ui/notifications", true).toBool());
}

void SettingsDialog::saveSettings()
{
    m_settings->setValue("osd/autoStart", m_autoStartOSDCheck->isChecked());
    m_settings->setValue("osd/updateInterval", m_updateIntervalSpin->value());
    m_settings->setValue("osd/opacity", m_opacitySlider->value());
    m_settings->setValue("connection/preference", m_connectionCombo->currentText());
    m_settings->setValue("ui/notifications", m_notificationsCheck->isChecked());
    
    m_settings->sync();
}

bool SettingsDialog::autoStartOSD() const
{
    return m_settings->value("osd/autoStart", false).toBool();
}

int SettingsDialog::osdUpdateInterval() const
{
    return m_settings->value("osd/updateInterval", 1000).toInt();
}

double SettingsDialog::osdOpacity() const
{
    return m_settings->value("osd/opacity", 90).toInt() / 100.0;
}

QString SettingsDialog::connectionPreference() const
{
    return m_settings->value("connection/preference", "Auto").toString();
}

bool SettingsDialog::enableNotifications() const
{
    return m_settings->value("ui/notifications", true).toBool();
}

void SettingsDialog::onAccepted()
{
    saveSettings();
    accept();
}

void SettingsDialog::onRejected()
{
    loadSettings(); // Restore previous values
    reject();
}

void SettingsDialog::onOpacityChanged(int value)
{
    m_opacityLabel->setText(QString("%1%").arg(value));
}
