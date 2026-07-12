#include <QtTest/QtTest>

#include "acquisition/WmiClient.h"

using namespace faraday;

class TestWmiClient : public QObject
{
    Q_OBJECT
private slots:
    void connectsToCimv2()
    {
        WmiClient client;
        QVERIFY2(client.connect(QStringLiteral("ROOT\\CIMV2")),
                 qPrintable(client.lastError()));
        QVERIFY(client.isConnected());
    }

    void queriesAlwaysPresentClass()
    {
        WmiClient client;
        QVERIFY(client.connect(QStringLiteral("ROOT\\CIMV2")));
        bool ok = false;
        const auto rows = client.query(
            QStringLiteral("SELECT Caption FROM Win32_OperatingSystem"), &ok);
        QVERIFY2(ok, qPrintable(client.lastError()));
        QVERIFY(!rows.isEmpty());
        QVERIFY(rows.first().contains(QStringLiteral("Caption")));
    }

    void missingClassFailsCleanly()
    {
        WmiClient client;
        QVERIFY(client.connect(QStringLiteral("ROOT\\CIMV2")));
        bool ok = true;
        const auto rows = client.query(
            QStringLiteral("SELECT * FROM Faraday_DoesNotExist"), &ok);
        QVERIFY(!ok);
        QVERIFY(rows.isEmpty());
        QVERIFY(!client.lastError().isEmpty());
    }

    void badNamespaceFailsCleanly()
    {
        WmiClient client;
        QVERIFY(!client.connect(QStringLiteral("ROOT\\FaradayNoSuchNamespace")));
        QVERIFY(!client.isConnected());
        QVERIFY(!client.lastError().isEmpty());
    }

    void queryWithoutConnectionFailsCleanly()
    {
        WmiClient client;
        bool ok = true;
        const auto rows = client.query(QStringLiteral("SELECT * FROM Win32_Battery"), &ok);
        QVERIFY(!ok);
        QVERIFY(rows.isEmpty());
    }

    void queryInstancesMatchesQuery()
    {
        WmiClient client;
        QVERIFY(client.connect(QStringLiteral("ROOT\\CIMV2")));
        bool ok = false;
        const auto rows = client.queryInstances(QStringLiteral("Win32_OperatingSystem"), &ok);
        QVERIFY2(ok, qPrintable(client.lastError()));
        QCOMPARE(rows.size(), 1);
        QVERIFY(rows.first().contains(QStringLiteral("Caption")));
    }

    void queryInstancesMissingClassFailsCleanly()
    {
        // Both the query path AND the CreateInstanceEnum retry must fail
        // for a class that does not exist, without crashing.
        WmiClient client;
        QVERIFY(client.connect(QStringLiteral("ROOT\\CIMV2")));
        bool ok = true;
        const auto rows = client.queryInstances(QStringLiteral("Faraday_DoesNotExist"), &ok);
        QVERIFY(!ok);
        QVERIFY(rows.isEmpty());
        QVERIFY(!client.lastError().isEmpty());
    }

    void queryInstancesWithoutConnectionFailsCleanly()
    {
        WmiClient client;
        bool ok = true;
        const auto rows = client.queryInstances(QStringLiteral("Win32_Battery"), &ok);
        QVERIFY(!ok);
        QVERIFY(rows.isEmpty());
    }

    void batteryStaticDataThroughFallbackPath()
    {
        // The class this whole mechanism exists for. On any machine the call
        // must complete cleanly; on battery hardware where the perf adapter
        // cooperates it must yield the design capacity.
        WmiClient wmi;
        if (!wmi.connect(QStringLiteral("ROOT\\WMI")))
            QSKIP("ROOT\\WMI unavailable on this machine");
        bool ok = false;
        const auto rows = wmi.queryInstances(QStringLiteral("BatteryStaticData"), &ok);
        if (ok && !rows.isEmpty()) {
            QVERIFY(rows.first().contains(QStringLiteral("InstanceName")));
            QVERIFY(rows.first().value(QStringLiteral("DesignedCapacity")).toUInt() > 0);
        }
        QVERIFY(true); // clean failure is acceptable; crashing is not
    }

    void batteryClassesNeverCrash()
    {
        // Whatever this machine is (laptop, desktop, VM), enumerating the
        // battery classes must complete without throwing or crashing.
        WmiClient wmi;
        if (wmi.connect(QStringLiteral("ROOT\\WMI"))) {
            const char *classes[] = { "BatteryStaticData", "BatteryFullChargedCapacity",
                                      "BatteryCycleCount", "BatteryStatus", "BatteryRuntime",
                                      "MsAcpi_ThermalZoneTemperature" };
            for (const char *cls : classes) {
                bool ok = false;
                const auto rows =
                    wmi.query(QStringLiteral("SELECT * FROM %1").arg(QLatin1String(cls)), &ok);
                Q_UNUSED(rows);
                Q_UNUSED(ok); // failure is acceptable; crashing is not
            }
        }
        QVERIFY(true);
    }
};

QTEST_APPLESS_MAIN(TestWmiClient)
#include "test_wmi_client.moc"
