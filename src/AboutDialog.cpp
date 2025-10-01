// ReSharper disable CppDFAMemoryLeak
#include "AboutDialog.h"
#include "lgplv3.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
  setupUI();
  setWindowTitle(tr("About USB Power OSD"));
  //setFixedSize(450, 250);
  setBaseSize(450, 250);
}

void AboutDialog::setupUI() {
  auto layout = new QVBoxLayout;

  auto titleLabel = new QLabel("<h2>USB Power OSD</h2>", this);
  titleLabel->setAlignment(Qt::AlignCenter);

  auto descriptionLabel =
      new QLabel("Monitor and display USB power consumption  data in real time.<br>"
                 "Version: 2.0.0<br>"
                 "Author: Thomas Lamy / MacWake.de",
                 this);
  descriptionLabel->setAlignment(Qt::AlignCenter);

  auto lgplLabel = new QLabel(
      "<small>This application uses Qt6, which is licensed under "
      "the <br>GNU Lesser General Public License (LGPL) version 3.<br>"
      "Qt6 source code is available at: https://code.qt.io/cgit/<br>"
      "Users have the right to modify Qt6 libraries and relink this application.</small>",
      this);
  lgplLabel->setAlignment(Qt::AlignCenter);

  auto okButton = new QPushButton(tr("OK"), this);
  auto lgplButton = new QPushButton(tr("LGPL V3"), this);
  connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(lgplButton, &QPushButton::clicked, [this] {
    auto dialog = new LgplDialog(this);
    dialog->exec();
  });

  layout->addWidget(titleLabel);
  layout->addWidget(descriptionLabel);
  layout->addWidget(lgplLabel);
  layout->addWidget(lgplButton, 0, Qt::AlignCenter);
  layout->addWidget(okButton, 0, Qt::AlignCenter);

  setLayout(layout);
}