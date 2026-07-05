#include "search-controller.hpp"

#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QTextStream>
#include <QTranslator>
#include <QUrl>

namespace
{

  void appendQmlLoadLog(const QString& message)
  {
    QFile file(QStringLiteral("uburu-qml-load-error.log"));

    if (!file.open(QIODevice::Append | QIODevice::Text))
      return;

    QTextStream output(&file);
    output << message << '\n';
  }

  void appendQmlWarnings(const QList<QQmlError>& warnings)
  {
    for (const auto& warning : warnings)
      appendQmlLoadLog(warning.toString());
  }

} // namespace

int main(int argc, char* argv[])
{
  QQuickStyle::setStyle(QStringLiteral("Basic"));

  QGuiApplication application(argc, argv);
  application.setOrganizationName(QStringLiteral("Uburu"));
  application.setApplicationName(QStringLiteral("Uburu"));
  application.setWindowIcon(QIcon(QStringLiteral(":/assets/logo-uburu.png")));

  QTranslator translator;
  const auto locale = QLocale::system().name().replace('_', '-').toLower();
  if (translator.load(QStringLiteral(":/i18n/uburu-%1.qm").arg(locale))) {
    application.installTranslator(&translator);
  }

  uburu::app::SearchController searchController;
  QQmlApplicationEngine engine;
  QObject::connect(&engine, &QQmlEngine::warnings, &application, appendQmlWarnings);
  engine.rootContext()->setContextProperty(QStringLiteral("searchController"), &searchController);
  engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
  if (engine.rootObjects().isEmpty()) {
    appendQmlLoadLog(QStringLiteral("Root QML object was not created."));
    return -1;
  }

  return application.exec();
}
