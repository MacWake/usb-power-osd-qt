#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include "OsdSettings.h"

#include <QDialog>
#include <QFontDialog>
#include <QSettings>
#include <qplaintextedit.h>

class MainWindow;
QT_BEGIN_NAMESPACE
class QCheckBox;
class QSpinBox;
class QComboBox;
class QSlider;
class QLabel;
QT_END_NAMESPACE

class SettingsDialog : public QDialog {
  Q_OBJECT

public:
  explicit SettingsDialog(OsdSettings *settings, QWidget *parent = nullptr);

private slots:
  void onAccepted();
  void onRejected();

private:
  void setupUI();
  QPushButton *createColorButton(const QString &name,
                                 const QString &displayName,
                                 QColor *settingsColor);
  void onColorChanged();

  OsdSettings *m_settings;

  QPushButton * m_priFontButton;
  QPushButton * m_secFontButton;
  QSpinBox *m_updateIntervalSpin;
  QSlider *m_opacitySlider;
  QLabel *m_opacityLabel;
  QComboBox *m_connectionCombo;
  QCheckBox *m_notificationsCheck;
  MainWindow * m_mainwindow;
  QSpinBox * m_minCurrent;
  QPushButton * m_BackgroundButton;
  QPushButton * m_TextButton;
  QPushButton * m_5VButton;
  QPushButton * m_9VButton;
  QPushButton * m_15VButton;
  QPushButton * m_20VButton;
  QPushButton * m_28VButton;
  QPushButton * m_36VButton;
  QPushButton * m_48VButton;
};

#endif // SETTINGS_H
