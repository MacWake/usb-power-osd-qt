//
// Created by Thomas Lamy on 23.09.25.
//

#ifndef USB_POWER_OSD_LGPLV3_H
#define USB_POWER_OSD_LGPLV3_H


#include <QDialog>

class LgplDialog : public QDialog {
  Q_OBJECT
public:
  explicit LgplDialog(QWidget *parent = nullptr);

private:
  void setupUI();
};

#endif // USB_POWER_OSD_LGPLV3_H
