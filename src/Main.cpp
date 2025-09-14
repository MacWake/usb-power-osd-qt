#include "MainWindow.h"
#include <QApplication>
#include <QDir>

// NOLINT(clang-tidy-static-accessed-through-instance)
int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  app.setApplicationName("USB Power OSD");
  app.setApplicationVersion("2.0.0");
  app.setOrganizationName("MacWake");
  app.setOrganizationDomain("de.macwake.usb-power-osd");

  // Set application icon
  app.setWindowIcon(QIcon(":/icons/app-icon.png"));

  MainWindow window;
  window.show();

  return app.exec();
}
