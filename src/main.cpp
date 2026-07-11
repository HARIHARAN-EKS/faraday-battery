#include "app/BatteryModel.h"
#include "app/TrayManager.h"

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTranslator>

int main(int argc, char *argv[])
{
    // QApplication (not QGuiApplication) so QSystemTrayIcon works; the UI
    // itself is pure Qt Quick.
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Faraday"));
    app.setOrganizationName(QStringLiteral("Faraday Project"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setQuitOnLastWindowClosed(false); // tray keeps us alive

    faraday::BatteryModel model;
    model.initialize();

    // English is the source language; other languages load from the
    // embedded :/i18n resources when their .qm ships.
    QTranslator translator;
    const QString language = model.settings()->language();
    if (language != QLatin1String("en")
        && translator.load(QStringLiteral(":/i18n/faraday_%1").arg(language))) {
        app.installTranslator(&translator);
    }

    faraday::TrayManager tray;
    QObject::connect(&model, &faraday::BatteryModel::alertRaised,
                     &tray, &faraday::TrayManager::showAlert);
    QObject::connect(&tray, &faraday::TrayManager::quitRequested,
                     &app, &QCoreApplication::quit);
    QObject::connect(&tray, &faraday::TrayManager::sampleNowRequested,
                     &model, &faraday::BatteryModel::sampleNow);
    QObject::connect(&model, &faraday::BatteryModel::snapshotChanged, &tray, [&]() {
        if (model.batteryPresent() && model.chargePercent() >= 0) {
            tray.setTooltip(QStringLiteral("Faraday — %1% · %2")
                                .arg(QString::number(model.chargePercent(), 'f', 0),
                                     model.statusText()));
        }
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("battery"), &model);
    engine.rootContext()->setContextProperty(QStringLiteral("tray"), &tray);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("Faraday", "Main");

    // Without a tray icon, closing the window must quit the app.
    if (!tray.available())
        app.setQuitOnLastWindowClosed(true);

    return app.exec();
}
