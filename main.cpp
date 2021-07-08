#include "mainwindow.hpp"
#include <QApplication>
#include <QStringList>

#include <QSqlDatabase>
#include <QSqlError>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  app.setApplicationName ("softrx");
  MainWindow w;
  w.show();
  return app.exec();
}
