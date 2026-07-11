#include "app/BatteryModel.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Faraday"));
    app.setOrganizationName(QStringLiteral("Faraday Project"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    faraday::BatteryModel model;
    model.initialize();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("battery"), &model);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Faraday", "Main");

    return app.exec();
}
