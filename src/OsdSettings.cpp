#include "OsdSettings.h"

#include "MainWindow.h"
#include "SettingsDialog.h"

#include <QApplication>
#include <QDebug>
#include <QtGui/qscreen.h>
#include <iostream>
#include <sstream>

OsdSettings::OsdSettings( // NOLINT(*-pro-type-member-init)
    const QString &organization, // NOLINT(*-pro-type-member-init)
    const QString &application, QObject *parent)
    : QSettings(organization, application, parent) {
  init();
}

void OsdSettings::init() {
  qreal scale = QApplication::primaryScreen()->devicePixelRatio();

  // Default settings
  always_on_top = false;
  is_line_graph = false;
  window_height = 300;
  window_width = 400;
  min_current = 0;
  volts_font_size = amps_font_size =
      static_cast<int>(static_cast<double>(24) * scale);
#if TARGET_OS_OSX
  measurements_font = "Monaco";
#elif TARGET_OS_WINDOWS
  measurements_font = "Arial";
#else
  measurements_font = "Roboto Mono";
#endif
  graph_height = 150;
  color_bg = QColor(0, 0, 0);
  color_amps = QColor(0xff, 0xff, 0xff);
  color_none = QColor(0xee, 0xee, 0xee);
  color_5v = QColor(0x00, 0xff, 0x00);
  color_9v = QColor(0x7f, 0xff, 0x00);
  color_15v = QColor(0x7f, 0x00, 0x7f);
  color_20v = QColor(0xff, 0xff, 0x00);
  color_28v = QColor(0xff, 0x00, 0x00);
  color_36v = QColor(0x00, 0xff, 0xff);
  color_48v = QColor(0x00, 0x00, 0xff);
  last_device = QString();
  loadSettings();
}

QColor OsdSettings::voltsRgb(PowerDelivery::PD_VOLTS volts) const {
  auto color = QColor(0xff, 0x33, 0x99);
  switch (volts) {
  case PowerDelivery::PD_NONE:
    color = color_none;
    break;
  case PowerDelivery::PD_5V:
    color = color_5v;
    break;
  case PowerDelivery::PD_9V:
    color = color_9v;
    break;
  case PowerDelivery::PD_15V:
    color = color_15v;
    break;
  case PowerDelivery::PD_20V:
    color = color_20v;
    break;
  case PowerDelivery::PD_28V:
    color = color_28v;
    break;
  case PowerDelivery::PD_36V:
    color = color_36v;
    break;
  case PowerDelivery::PD_48V:
    color = color_48v;
    break;
  default:
    // qWarning() << "Unknown voltage enum "<<volts;
    break;
  }
  return color;
}

void OsdSettings::saveSettings() {
  setValue("view/always_on_top", always_on_top);
  setValue("view/always_on_top", this->always_on_top);
  setValue("view/is_line_graph", this->is_line_graph);
  setValue("window/height", this->window_height);
  setValue("window/width", this->window_width);
  setValue("window/graph_height", this->graph_height);
  setValue("measurement/font", this->measurements_font);
  setValue("measurement/volts_font_size", this->volts_font_size);
  setValue("measurement/amps_font_size", this->amps_font_size);
  setValue("measurement/min_current", this->min_current);
  setValue("colors/background", this->color_bg);
  setValue("colors/amps", this->color_amps);
  setValue("colors/5v", this->color_5v);
  setValue("colors/9v", this->color_9v);
  setValue("colors/15v", this->color_15v);
  setValue("colors/20v", this->color_20v);
  setValue("colors/28v", this->color_28v);
  setValue("colors/36v", this->color_36v);
  setValue("colors/48v", this->color_48v);
  setValue("device/last", this->last_device);

  // Ensure settings are written to disk
  sync();
}

QColor OsdSettings::colorValue(const QString &key, QColor default_color) {
  if (this->contains(key)) {
    return QColor(value(key).toString());
  }
  return default_color;
}
void OsdSettings::loadSettings() {
  this->always_on_top =
      value("view/always_on_top", this->always_on_top).toBool();
  this->is_line_graph =
      value("view/is_line_graph", this->is_line_graph).toBool();
  this->window_height = value("window/height", this->window_height).toInt();
  this->window_width = value("window/width", this->window_width).toInt();
  this->graph_height = value("window/graph_height", this->graph_height).toInt();
  this->measurements_font =
      value("measurement/font", this->measurements_font).toString();
  this->volts_font_size =
      value("measurement/volts_font_size", this->volts_font_size).toInt();
  this->amps_font_size =
      value("measurement/amps_font_size", this->amps_font_size).toInt();
  this->min_current =
      value("measurement/min_current", this->min_current).toFloat();

  this->color_amps = colorValue("colors/amps", this->color_amps);
  this->color_5v = colorValue("colors/5v", this->color_5v);
  this->color_9v = colorValue("colors/9v", this->color_9v);
  this->color_15v = colorValue("colors/15v", this->color_15v);
  this->color_20v = colorValue("colors/20v", this->color_20v);
  this->color_28v = colorValue("colors/28v", this->color_28v);
  this->color_36v = colorValue("colors/36v", this->color_36v);
  this->color_48v = colorValue("colors/48v", this->color_48v);
  this->last_device = value("device/last", this->last_device).toString();
}
