#include "search-controller.hpp"

#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTranslator>
#include <QUrl>

int main(int argc, char* argv[])
{
  QGuiApplication application(argc, argv);
  application.setOrganizationName(QStringLiteral("Uburu"));
  application.setApplicationName(QStringLiteral("Uburu"));

  QTranslator translator;
  const auto locale = QLocale::system().name().replace('_', '-').toLower();
  if (translator.load(QStringLiteral(":/i18n/uburu-%1.qm").arg(locale))) {
    application.installTranslator(&translator);
  }

  uburu::app::SearchController searchController;
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("searchController"), &searchController);
  engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
  if (engine.rootObjects().isEmpty())
    return -1;
  return application.exec();
}
