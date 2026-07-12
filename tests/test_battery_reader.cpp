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

    void chemistryFourCC()
    {
        // 0x6E6F694C = 'L','i','o','n' little-endian (observed on real HP hardware).
        QCOMPARE(BatteryReader::chemistryFourCCToString(0x6E6F694Cu),
                 QStringLiteral("Lithium-ion"));
        // "LiP " (with trailing space trimmed by normalize) and "PbAc"
        QCOMPARE(BatteryReader::chemistryFourCCToString(0x0050694Cu), // "LiP\0"
                 QStringLiteral("Lithium Polymer"));
        QCOMPARE(BatteryReader::chemistryFourCCToString(0x63416250u), // "PbAc"
                 QStringLiteral("Lead Acid"));
        QVERIFY(BatteryReader::chemistryFourCCToString(0).isEmpty());
        // Non-printable garbage must not be guessed at.
        QVERIFY(BatteryReader::chemistryFourCCToString(0x01020304u).isEmpty());
        // Unknown but printable token passes through verbatim.
        QCOMPARE(BatteryReader::chemistryFourCCToString(0x44434241u), // "ABCD"
                 QStringLiteral("ABCD"));
    }

    void chemistryTokenNormalization()
    {
        QCOMPARE(BatteryReader::normalizeChemistryToken(QStringLiteral("LIon")),
                 QStringLiteral("Lithium-ion"));
        QCOMPARE(BatteryReader::normalizeChemistryToken(QStringLiteral("Lion")),
                 QStringLiteral("Lithium-ion"));
        QCOMPARE(BatteryReader::normalizeChemistryToken(QStringLiteral("LiP")),
                 QStringLiteral("Lithium Polymer"));
        QCOMPARE(BatteryReader::normalizeChemistryToken(QStringLiteral("NiMH")),
                 QStringLiteral("Nickel Metal Hydride"));
        QVERIFY(BatteryReader::normalizeChemistryToken(QString()).isEmpty());
        QCOMPARE(BatteryReader::normalizeChemistryToken(QStringLiteral("Exotic")),
                 QStringLiteral("Exotic"));
    }

    void manufactureDateParsing()
    {
        // Valid CIM datetime.
        QCOMPARE(BatteryReader::manufactureDateToIso(
                     QStringLiteral("20250821000000.000000+000")),
                 QStringLiteral("2025-08-21"));
        // All-wildcard datetime (the common "not reported" shape, observed
        // on the reference machine) parses to empty — never a guess.
        QVERIFY(BatteryReader::manufactureDateToIso(
                    QStringLiteral("**************.**********")).isEmpty());
        QVERIFY(BatteryReader::manufactureDateToIso(QString()).isEmpty());
        QVERIFY(BatteryReader::manufactureDateToIso(QStringLiteral("garbage")).isEmpty());
        // Implausible year and invalid month/day are rejected.
        QVERIFY(BatteryReader::manufactureDateToIso(
                    QStringLiteral("19000101000000.000000+000")).isEmpty());
        QVERIFY(BatteryReader::manufactureDateToIso(
                    QStringLiteral("20251340000000.000000+000")).isEmpty());
    }

    void win32StatusMapping()
    {
        QCOMPARE(BatteryReader::win32StatusToString(1), QStringLiteral("Discharging"));
        QCOMPARE(BatteryReader::win32StatusToString(2), QStringLiteral("On AC power"));
        QCOMPARE(BatteryReader::win32StatusToString(11), QStringLiteral("Partially charged"));
        QVERIFY(BatteryReader::win32StatusToString(0).isEmpty());
        QVERIFY(BatteryReader::win32StatusToString(12).isEmpty());
    }

    void temperatureResolutionPrefersBatteryZone()
    {
        BatterySnapshot snap;
        ThermalZone cpu;
        cpu.instanceName = QStringLiteral("ACPI\\ThermalZone\\CPUZ_0");
        cpu.celsius = 48.0;
        cpu.valid = true;
        ThermalZone bat;
        bat.instanceName = QStringLiteral("ACPI\\ThermalZone\\BATZ_0");
        bat.celsius = 33.5;
        bat.valid = true;
        bat.isBatteryZone = true;
        snap.thermalZones << cpu << bat;

        BatteryReader::resolveTemperature(snap);
        QVERIFY(snap.temperatureC.has_value());
        QCOMPARE(*snap.temperatureC, 33.5);       // battery zone, not the mean
        QVERIFY(!snap.temperatureIsEstimate);     // a real battery sensor
    }

    void temperatureResolutionFallsBackToEstimate()
    {
        // No *BAT* zone (the reference machine's shape): the mean of the
        // valid zones is reported, flagged as an estimate; the TZ1-style
        // stub zone (raw 2732 = 0.05 C -> valid=false) is excluded.
        BatterySnapshot snap;
        ThermalZone tz0;
        tz0.instanceName = QStringLiteral("ACPI\\ThermalZone\\TZ0__0");
        tz0.celsius = 40.0;
        tz0.valid = true;
        ThermalZone tz1Stub;
        tz1Stub.instanceName = QStringLiteral("ACPI\\ThermalZone\\TZ1__0");
        tz1Stub.celsius = BatteryReader::thermalRawToCelsius(2732);
        tz1Stub.valid = BatteryReader::thermalRawIsValid(2732); // false
        ThermalZone tz2;
        tz2.instanceName = QStringLiteral("ACPI\\ThermalZone\\TZ2__0");
        tz2.celsius = 50.0;
        tz2.valid = true;
        snap.thermalZones << tz0 << tz1Stub << tz2;

        BatteryReader::resolveTemperature(snap);
        QVERIFY(snap.temperatureC.has_value());
        QCOMPARE(*snap.temperatureC, 45.0);       // mean of 40 and 50 only
        QVERIFY(snap.temperatureIsEstimate);      // honestly flagged
    }

    void temperatureResolutionRejectsStubsAndEmpty()
    {
        BatterySnapshot onlyStubs;
        ThermalZone stub;
        stub.instanceName = QStringLiteral("ACPI\\ThermalZone\\TZ1__0");
        stub.valid = false;
        onlyStubs.thermalZones << stub;
        BatteryReader::resolveTemperature(onlyStubs);
        QVERIFY(!onlyStubs.temperatureC.has_value());
        QVERIFY(!onlyStubs.unavailable.filter(QStringLiteral("stub")).isEmpty());

        BatterySnapshot none;
        BatteryReader::resolveTemperature(none);
        QVERIFY(!none.temperatureC.has_value());
        QVERIFY(!none.unavailable.filter(QStringLiteral("no instances")).isEmpty());
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
