#include <QtTest/QtTest>

#include "app/AlertManager.h"
#include "data/Settings.h"

#include <QDir>

using namespace faraday;

class TestAlerts : public QObject
{
    Q_OBJECT

    QString m_dir;
    Settings *m_settings = nullptr;

    AlertInput baseline()
    {
        AlertInput input;
        input.percent = 60;
        input.voltageV = 12.6;
        input.temperatureC = 30.0;
        input.charging = false;
        input.discharging = true;
        input.onAcPower = false;
        return input;
    }

    static QStringList ids(const QList<Alert> &alerts)
    {
        QStringList out;
        for (const Alert &a : alerts)
            out << a.id;
        return out;
    }

private slots:
    void initTestCase()
    {
        m_dir = QDir::temp().filePath(QStringLiteral("faraday_test_alerts"));
        QDir().mkpath(m_dir);
        m_settings = new Settings(m_dir); // defaults: low 20, temp 50, mv 10500
    }

    void noAlertsInNormalConditions()
    {
        AlertManager mgr(m_settings);
        QVERIFY(mgr.evaluate(baseline()).isEmpty());
    }

    void lowLevelLatchesAndRearms()
    {
        AlertManager mgr(m_settings);
        AlertInput input = baseline();
        input.percent = 15; // below 20: lowLevel AND chargeBelow share the default threshold
        QVERIFY(ids(mgr.evaluate(input)).contains(QStringLiteral("lowLevel")));
        QVERIFY(mgr.evaluate(input).isEmpty());          // latched: no repeat
        input.percent = 22;                              // inside hysteresis band
        QVERIFY(mgr.evaluate(input).isEmpty());
        input.percent = 30;                              // above 25 -> re-arm
        QVERIFY(mgr.evaluate(input).isEmpty());
        input.percent = 12;
        QVERIFY(ids(mgr.evaluate(input)).contains(QStringLiteral("lowLevel")));
    }

    void lowLevelIgnoredOnAc()
    {
        AlertManager mgr(m_settings);
        AlertInput input = baseline();
        input.percent = 10;
        input.onAcPower = true;
        input.charging = true;
        // No lowLevel/chargeBelow on AC (unplug reminder needs >= 95 %)
        QVERIFY(mgr.evaluate(input).isEmpty());
    }

    void highTempFires()
    {
        AlertManager mgr(m_settings);
        AlertInput input = baseline();
        input.temperatureC = 55.0;
        QCOMPARE(ids(mgr.evaluate(input)), QStringList{ QStringLiteral("highTemp") });
        QVERIFY(mgr.evaluate(input).isEmpty()); // latched
        input.temperatureC = 44.0;              // below 47 -> re-arm
        QVERIFY(mgr.evaluate(input).isEmpty());
        input.temperatureC = 51.0;
        QCOMPARE(ids(mgr.evaluate(input)), QStringList{ QStringLiteral("highTemp") });
    }

    void lowVoltageFires()
    {
        AlertManager mgr(m_settings);
        AlertInput input = baseline();
        input.voltageV = 10.2; // below 10.5
        QCOMPARE(ids(mgr.evaluate(input)), QStringList{ QStringLiteral("lowVoltage") });
        QVERIFY(mgr.evaluate(input).isEmpty());
    }

    void unplugAtFullFires()
    {
        AlertManager mgr(m_settings);
        AlertInput input = baseline();
        input.onAcPower = true;
        input.charging = true;
        input.discharging = false;
        input.percent = 97; // above 95 default
        QCOMPARE(ids(mgr.evaluate(input)), QStringList{ QStringLiteral("unplugAtFull") });
        QVERIFY(mgr.evaluate(input).isEmpty()); // latched
        input.onAcPower = false;                // unplugged -> re-arm
        input.percent = 90;
        QVERIFY(mgr.evaluate(input).isEmpty());
        input.onAcPower = true;
        input.percent = 96;
        QCOMPARE(ids(mgr.evaluate(input)), QStringList{ QStringLiteral("unplugAtFull") });
    }

    void chargeBelowFires()
    {
        AlertManager mgr(m_settings);
        AlertInput input = baseline();
        input.percent = 18; // below default 20 -> chargeBelow AND lowLevel
        const QStringList fired = ids(mgr.evaluate(input));
        QVERIFY(fired.contains(QStringLiteral("chargeBelow")));
        QVERIFY(fired.contains(QStringLiteral("lowLevel")));
    }

    void masterSwitchSilencesEverything()
    {
        Settings muted(m_dir + QStringLiteral("/muted"));
        muted.setValue(QStringLiteral("alertsEnabled"), false);
        AlertManager mgr(&muted);
        AlertInput input = baseline();
        input.percent = 5;
        input.temperatureC = 70.0;
        input.voltageV = 9.0;
        QVERIFY(mgr.evaluate(input).isEmpty());
    }

    void signalEmission()
    {
        AlertManager mgr(m_settings);
        QSignalSpy spy(&mgr, &AlertManager::alertRaised);
        AlertInput input = baseline();
        input.temperatureC = 60.0;
        mgr.process(input);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!spy.first().at(0).toString().isEmpty());
        QVERIFY(!spy.first().at(1).toString().isEmpty());
    }

    void cleanupTestCase()
    {
        delete m_settings;
        QDir(m_dir).removeRecursively();
    }
};

QTEST_APPLESS_MAIN(TestAlerts)
#include "test_alerts.moc"
