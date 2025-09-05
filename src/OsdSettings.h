//
// Created by Thomas Lamy on 14.03.25.
//

#ifndef SETTINGS_H
#define SETTINGS_H

#include "PowerDelivery.h"

#include <QSettings>
#include <QtGui/qcolor.h>
#include <QtWidgets/qstyle.h>

class OsdSettings : public QSettings {
  Q_OBJECT
public:
  bool always_on_top = false;
  bool is_line_graph = false;
  int window_top;
  int window_left;
  int window_height;
  int window_width;
  QString measurements_font;
  int volts_font_size;
  int amps_font_size;
  int power_font_size;
  int energy_font_size;
  int graph_height;
  float min_current = 0.0;
  QColor color_bg;
  QColor color_amps;
  QColor color_none;
  QColor color_5v;
  QColor color_9v;
  QColor color_15v;
  QColor color_20v;
  QColor color_28v;
  QColor color_36v;
  QColor color_48v;
  QString last_device;

  OsdSettings(const QString &organization, const QString &application,
              QObject *parent);
  void init();

  QColor voltsRgb(PowerDelivery::PD_VOLTS volts) const;

  void saveSettings();

  void loadSettings();

private:
  static QColor setting2Rgb(const QString &setting);
  static QString rgb_to_string(const QColor &rgb);
};

#endif // SETTINGS_H
