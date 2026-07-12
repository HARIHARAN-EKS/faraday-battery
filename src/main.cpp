#include "app/BatteryModel.h"
#include "app/PortableMode.h"
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
    app.setApplicationVersion(QStringLiteral("1.0.3"));
    app.setQuitOnLastWindowClosed(false); // tray keeps us alive

    faraday::BatteryModel model;
    // Portable ZIP: a portable.txt marker next to the exe keeps all data
    // in <exeDir>\data so the app travels with its folder (USB included).
    // Empty means the default per-user location.
    model.initialize(
        faraday::PortableMode::dataDirFor(QCoreApplication::applicationDirPath()));

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

    // objectCreationFailed does NOT fire when a QML module *plugin* fails to
    // load (e.g. a missing Qt DLL in a broken deployment) — the engine just
    // produces no root objects. Without this check the tray-driven event
    // loop would keep a windowless zombie process alive. Verified against a
    // deliberately broken dist.
    if (engine.rootObjects().isEmpty())
        return 1;

    // Without a tray icon, closing the window must quit the app.
    if (!tray.available())
        app.setQuitOnLastWindowClosed(true);

    return app.exec();
}
