#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "CurrentGraph.h"
#include "DeviceManager.h"
#include "MeasurementHistory.h"
#include "OsdSettings.h"
#include "PowerMonitor.h"
#include "SettingsDialog.h"
#include "DeviceSelectionDialog.h"
#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTimer>

QT_BEGIN_NAMESPACE
class QLabel;
class QProgressBar;
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();
  void showStatusMessage(const QString &message, int hideAfterMs);

private slots:
  void onPowerDataReceived(const PowerData &data);
  void onDeviceConnected(const QString &deviceName);
  void onDeviceDisconnected();
  void showSettings();
  void updateLabels();
  void hideStatusBar();
  void connectLastDevice();
  void showDeviceSelectionDialog();
  void resetMeasurementHistory();

protected:
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void setupUI();
  void updateUINoData();
  void setBackgroundColor(const QColor &color);
  void positionWidgets();

  PowerMonitor *m_powerMonitor;
  DeviceManager *m_deviceManager;
  SettingsDialog *m_settings;
  OsdSettings *settings = nullptr;
  MeasurementHistory *m_history = nullptr;
  DeviceSelectionDialog *m_deviceSelectionDialog;

  QTimer *m_updateTimer;
  QTimer *m_statusBarHideTimer;

  // UI members
  QLabel *lblVoltage;
  QLabel *lblCurrent;
  QLabel *lblPower;
  QLabel *lblEnergy;
  QLabel *lblMinMaxCurrent;
  QFont *fntVoltage;
  QFont *fntCurrent;
  QFont *fntPower;
  QFont *fntEnergy;

  CurrentGraph *m_currentGraph;
};

#endif // MAINWINDOW_H
