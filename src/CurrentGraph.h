#pragma once
#include "MeasurementHistory.h"
#include "OsdSettings.h"

#include <QTimer>
#include <QWidget>

class CurrentGraph : public QWidget {
    Q_OBJECT
public:
    explicit CurrentGraph(QWidget* parent, MeasurementHistory *history, OsdSettings *settings);

protected:
    double findLowBox(double min_current);
  double findHighBox(double max_current);
  void paintEvent(QPaintEvent* event) override;
private slots:
    void updateGraph();
private:
    MeasurementHistory *history;
    OsdSettings * settings;
    QTimer *m_refreshTimer;
};
