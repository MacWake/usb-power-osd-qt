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
    bool is_energy_displayed = false;
    int window_top;
    int window_left;
    int window_height;
    int window_width;
    QString primary_font_name;
    int primary_font_size;
    QString secondary_font_name;
    int secondary_font_size;
    float min_current = 0.0;
    int current_diff_ma = 0;
    QColor color_bg;
    QColor color_text;
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

    QColor colorValue(const QString &key, QColor default_color) const;

    void loadSettings();
};

#endif // SETTINGS_H
