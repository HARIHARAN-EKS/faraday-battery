#include <QtTest/QtTest>

#include "acquisition/BatteryReader.h"

using namespace faraday;

class TestBatteryReader : public QObject
{
    Q_OBJECT
private slots:
    void thermalConversion()
    {
        // 3372 tenths of Kelvin -> 64.05 C ; 2732 -> 0.05 C (stub)
        QCOMPARE(BatteryReader::thermalRawToCelsius(2732), 0.05);
        QVERIFY(qAbs(BatteryReader::thermalRawToCelsius(3372) - 64.05) < 1e-9);
        QVERIFY(qAbs(BatteryReader::thermalRawToCelsius(2982) - 25.05) < 1e-9);
    }

    void thermalValidity()
    {
        QVERIFY(!BatteryReader::thermalRawIsValid(0));
        QVERIFY(!BatteryReader::thermalRawIsValid(2732));  // 0.05 C firmware stub
        QVERIFY(BatteryReader::thermalRawIsValid(2932));   // 20.05 C
        QVERIFY(BatteryReader::thermalRawIsValid(3372));   // 64.05 C
        QVERIFY(!BatteryReader::thermalRawIsValid(4200));  // 146.85 C absurd
    }

    void runtimeSentinelFiltering()
    {
        QVERIFY(!BatteryReader::sanitizeRuntime(0xFFFFFFFFu).has_value()); // "unknown"
        QVERIFY(!BatteryReader::sanitizeRuntime(0).has_value());
        QVERIFY(!BatteryReader::sanitizeRuntime(60u * 60u * 24u * 30u).has_value());
        QCOMPARE(BatteryReader::sanitizeRuntime(7200).value_or(0), quint32(7200));
    }

    void chemistryMapping()
    {
        QCOMPARE(BatteryReader::chemistryToString(6), QStringLiteral("Lithium-ion"));
        QCOMPARE(BatteryReader::chemistryToString(8), QStringLiteral("Lithium Polymer"));
        QVERIFY(BatteryReader::chemistryToString(0).isEmpty());
        QVERIFY(BatteryReader::chemistryToString(99).isEmpty());
    }

    void win32StatusMapping()
    {
        QCOMPARE(BatteryReader::win32StatusToString(1), QStringLiteral("Discharging"));
        QCOMPARE(BatteryReader::win32StatusToString(2), QStringLiteral("On AC power"));
        QCOMPARE(BatteryReader::win32StatusToString(11), QStringLiteral("Partially charged"));
        QVERIFY(BatteryReader::win32StatusToString(0).isEmpty());
        QVERIFY(BatteryReader::win32StatusToString(12).isEmpty());
    }

    void noBatteryDetection()
    {
        BatterySnapshot empty;
        QVERIFY(!BatteryReader::snapshotHasRealBattery(empty));

        // A VM ghost battery with zeroed capacities is not "present".
        BatterySnapshot ghost;
        BatteryDevice ghostDev;
        ghostDev.instanceName = QStringLiteral("ACPI\\PNP0C0A\\0");
        ghostDev.fullChargedCapacitymWh = 0;
        ghostDev.designedCapacitymWh = 0;
        ghost.batteries.append(ghostDev);
        QVERIFY(!BatteryReader::snapshotHasRealBattery(ghost));

        BatterySnapshot real;
        BatteryDevice realDev;
        realDev.fullChargedCapacitymWh = 42401;
        real.batteries.append(realDev);
        QVERIFY(BatteryReader::snapshotHasRealBattery(real));
    }

    void liveSnapshotNeverCrashes()
    {
        // Integration path on whatever hardware runs the tests: read() must
        // return a coherent snapshot without crashing, battery or not.
        BatteryReader reader;
        const bool init = reader.initialize();
        QVERIFY(init); // at least one namespace must connect on any Windows box

        const BatterySnapshot snap = reader.read();
        QVERIFY(snap.timestamp.isValid());
        if (snap.batteryPresent) {
            QVERIFY(!snap.batteries.isEmpty());
            for (const BatteryDevice &b : snap.batteries) {
                if (b.remainingCapacitymWh && b.fullChargedCapacitymWh
                    && *b.fullChargedCapacitymWh > 0) {
                    // Allow slight over-report right at full charge.
                    QVERIFY(*b.remainingCapacitymWh <= *b.fullChargedCapacitymWh * 11 / 10);
                }
            }
        }
    }
};

QTEST_APPLESS_MAIN(TestBatteryReader)
#include "test_battery_reader.moc"
