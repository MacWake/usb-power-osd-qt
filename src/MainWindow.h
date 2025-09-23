#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "CurrentGraph.h"
#include "DeviceManager.h"
#include "DeviceSelectionDialog.h"
#include "MeasurementHistory.h"
#include "OsdSettings.h"
#include "PowerMonitor.h"
#include "SettingsDialog.h"
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
  void tryConnectLastDevice();
  explicit MainWindow(OsdSettings *settings, QWidget *parent = nullptr);
  ~MainWindow() override;
  void startReconnectTimer();
  void showStatusMessage(const QString &message, int hideAfterMs);

  // Public getter for settings
  [[nodiscard]] OsdSettings *getSettings() const { return settings; }
  void onPrimaryFontChanged(const QFont &font);
  void onSecondaryFontChanged(const QFont &font);
  void onColorChanged();

private slots:
  void onPowerDataReceived(const PowerData &data);
  void onDeviceConnected(const QString &deviceName);
  void onDeviceDisconnected();
  void showSettings();
  void updateLabels();
  void hideStatusBar();
  void connectLastDevice(bool reconnecting);
  void showDeviceSelectionDialog();
  void resetMeasurementHistory();
  void showAboutDialog();


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
  SettingsDialog *m_settingsdialog;
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
  QFont *fntPrimary;
  QFont *fntSecondary;

  CurrentGraph *m_currentGraph;
  QTimer *m_reconnect_timer;
};

#endif // MAINWINDOW_H
