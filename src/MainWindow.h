#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "DeviceManager.h"
#include "OsdSettings.h"
#include "PowerMonitor.h"
#include "SettingsDialog.h"
#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QLabel;
class QProgressBar;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onPowerDataReceived(const PowerData &data);
    void onDeviceConnected(const QString &deviceName);
    void onDeviceDisconnected();
    void showSettings();

private:
    void setupUI();
    void updateUI(const PowerData &data);
    
    PowerMonitor *m_powerMonitor;
    DeviceManager *m_deviceManager;
    SettingsDialog *m_settings;
    OsdSettings *settings = nullptr;

    QLabel *m_statusLabel;
    QLabel *m_voltageLabel;
    QLabel *m_currentLabel;
    QLabel *m_powerLabel;
    QLabel *m_energyLabel;
    QProgressBar *m_voltageBar;
    QProgressBar *m_currentBar;

    QTimer *m_updateTimer;
};

#endif // MAINWINDOW_H
