#include <QtTest/QtTest>

#include "PropertyGen.h"
#include "acquisition/BatteryReader.h"

#include <QRegularExpression>

using namespace faraday;
using namespace faraday::propgen;

namespace {

// Random QVariant of a random (frequently wrong) type.
QVariant randomVariant(Gen &g)
{
    switch (g.u32Below(10)) {
    case 0: return QVariant();                                    // null
    case 1: return QVariant(int(g.i32In(INT32_MIN, INT32_MAX)));  // int
    case 2: return QVariant(quint64(g.u32()) << 32 | g.u32());    // huge u64
    case 3: return QVariant(g.dblIn(-1e18, 1e18));                // double
    case 4: return QVariant(bool(g.chance(0.5)));                 // bool
    case 5: {                                                      // garbage string
        QString s;
        const int n = int(g.u32Below(64));
        for (int i = 0; i < n; ++i)
            s.append(QChar(ushort(g.u32Below(0xFFFF))));
        return QVariant(s);
    }
    case 6: return QVariant(QByteArray("\xde\xad\xbe\xef", 4));   // bytes
    case 7: return QVariant(QVariantList{ 1, 2, 3 });             // list
    case 8: return QVariant(QVariantMap{ { QStringLiteral("k"), 1 } }); // map
    default: return QVariant(QString::number(g.u32()));           // numeric string
    }
}

QVariantMap randomRow(Gen &g, const QStringList &schema, double keepInstanceP = 0.8)
{
    QVariantMap row;
    if (g.chance(keepInstanceP))
        row.insert(QStringLiteral("InstanceName"),
                   QStringLiteral("ACPI\\PNP0C0A\\%1_0").arg(g.u32Below(4)));
    for (const QString &key : schema) {
        if (g.chance(0.7))
            row.insert(key, randomVariant(g));
    }
    return row;
}

} // namespace

// Fuzzing of the WMI row-application layer and the pure decoders: hostile
// property maps (wrong types, garbage, partial rows, duplicate instances)
// must never crash and never produce out-of-contract snapshots.
class TestFuzzReader : public QObject
{
    Q_OBJECT
private slots:
    void staticRowsGarbage_data()
    {
        QTest::addColumn<int>("seed");
        for (int i = 0; i < 150; ++i)
            QTest::addRow("static%03d", i) << i;
    }
    void staticRowsGarbage()
    {
        QFETCH(int, seed);
        Gen g(0xF122ADE1u + seed);
        const QStringList schema = { "DesignedCapacity", "ManufactureName", "SerialNumber",
                                     "DeviceName", "UniqueID", "ManufactureDate", "Chemistry" };
        QList<QVariantMap> rows;
        const int n = int(g.u32Below(8));
        for (int i = 0; i < n; ++i)
            rows.append(randomRow(g, schema));

        BatterySnapshot snap;
        BatteryReader::applyStaticRows(snap, rows); // must not crash
        QVERIFY(snap.batteries.size() <= 4);        // keyed by 4 instance names max
        for (const BatteryDevice &d : snap.batteries) {
            QVERIFY(!d.instanceName.isEmpty());     // rows without a key are dropped
            // Manufacture date is either absent or a real ISO date.
            if (!d.manufactureDate.isEmpty()) {
                QVERIFY(QRegularExpression(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"))
                            .match(d.manufactureDate).hasMatch());
            }
            // Chemistry is either absent or printable text.
            for (const QChar &c : d.chemistry)
                QVERIFY(c.isPrint() || c.isSpace());
        }
    }

    void rootWmiClassRowsGarbage_data()
    {
        QTest::addColumn<int>("seed");
        QTest::addColumn<QByteArray>("className");
        const QList<QByteArray> classes = { "BatteryFullChargedCapacity", "BatteryCycleCount",
                                            "BatteryStatus", "BatteryRuntime",
                                            "TotallyUnknownClass" };
        int row = 0;
        for (const QByteArray &cls : classes)
            for (int i = 0; i < 30; ++i)
                QTest::addRow("cls=%s %02d", cls.constData(), i) << row++ << cls;
    }
    void rootWmiClassRowsGarbage()
    {
        QFETCH(int, seed);
        QFETCH(QByteArray, className);
        Gen g(0xF122ADE2u + seed);
        const QStringList schema = { "FullChargedCapacity", "CycleCount", "RemainingCapacity",
                                     "ChargeRate", "DischargeRate", "Voltage", "PowerOnline",
                                     "Charging", "Discharging", "Critical", "EstimatedRuntime" };
        QList<QVariantMap> rows;
        const int n = int(g.u32Below(6));
        for (int i = 0; i < n; ++i)
            rows.append(randomRow(g, schema));

        BatterySnapshot snap;
        BatteryReader::applyRootWmiClassRows(snap, className, rows);
        QVERIFY(snap.batteries.size() <= 4);
        for (const BatteryDevice &d : snap.batteries) {
            // Sentinel runtime never survives sanitation, whatever the input.
            if (d.estimatedRuntimeSec.has_value()) {
                QVERIFY(*d.estimatedRuntimeSec > 0);
                QVERIFY(*d.estimatedRuntimeSec <= 60u * 60u * 24u * 7u);
            }
        }
        if (className == "TotallyUnknownClass" && n > 0) {
            // Unknown classes create the keyed devices but set no fields.
            for (const BatteryDevice &d : snap.batteries) {
                QVERIFY(!d.fullChargedCapacitymWh.has_value());
                QVERIFY(!d.remainingCapacitymWh.has_value());
            }
        }
    }

    void cimv2RowsGarbage_data()
    {
        QTest::addColumn<int>("seed");
        QTest::addColumn<int>("win32Count");
        QTest::addColumn<int>("portableCount");
        QTest::addColumn<int>("preexistingPacks");
        Gen g(0xF122ADE3u);
        for (int i = 0; i < 100; ++i) {
            QTest::addRow("cimv2-%03d", i)
                << i << int(g.u32Below(6)) << int(g.u32Below(6)) << int(g.u32Below(3));
        }
    }
    void cimv2RowsGarbage()
    {
        QFETCH(int, seed);
        QFETCH(int, win32Count);
        QFETCH(int, portableCount);
        QFETCH(int, preexistingPacks);
        Gen g(0xF122ADE4u + seed);
        const QStringList schema = { "Name", "BatteryStatus", "EstimatedChargeRemaining",
                                     "DesignVoltage", "Chemistry", "Manufacturer",
                                     "DesignCapacity", "FullChargeCapacity", "DeviceID" };
        BatterySnapshot snap;
        for (int i = 0; i < preexistingPacks; ++i) {
            BatteryDevice d;
            d.instanceName = QStringLiteral("ACPI\\PNP0C0A\\%1_0").arg(i);
            snap.batteries.append(d);
        }
        QList<QVariantMap> win32, portable;
        for (int i = 0; i < win32Count; ++i)
            win32.append(randomRow(g, schema, 0.0));
        for (int i = 0; i < portableCount; ++i)
            portable.append(randomRow(g, schema, 0.0));

        BatteryReader::applyCimv2Rows(snap, win32, portable); // must not crash
        // Mismatched list sizes never create more packs than max(list sizes)
        // beyond the preexisting ones.
        QVERIFY(snap.batteries.size() <= preexistingPacks + qMax(win32Count, portableCount));
        for (const BatteryDevice &d : snap.batteries)
            QVERIFY(!d.instanceName.isEmpty());
    }

    void thermalZoneRowsGarbage_data()
    {
        QTest::addColumn<int>("seed");
        for (int i = 0; i < 80; ++i)
            QTest::addRow("zone%02d", i) << i;
    }
    void thermalZoneRowsGarbage()
    {
        QFETCH(int, seed);
        Gen g(0xF122ADE5u + seed);
        QVariantMap row = randomRow(g, { QStringLiteral("CurrentTemperature") });
        const ThermalZone zone = BatteryReader::zoneFromRow(row);
        // A zone marked valid ALWAYS carries a physically plausible reading.
        if (zone.valid)
            QVERIFY(zone.celsius > 0.0 && zone.celsius <= 120.0);
        // And a snapshot built from any zone mix keeps the temperature contract.
        BatterySnapshot snap;
        snap.thermalZones.append(zone);
        BatteryReader::resolveTemperature(snap);
        if (snap.temperatureC.has_value()) {
            QVERIFY(*snap.temperatureC > 0.0 && *snap.temperatureC <= 120.0);
            QVERIFY(zone.valid); // temperature only ever comes from valid zones
        }
    }

    void staticCachePolicy()
    {
        BatteryReader reader; // never initialize() — no WMI in this test
        QVariantMap row;
        row.insert(QStringLiteral("InstanceName"), QStringLiteral("ACPI\\PNP0C0A\\1_0"));
        row.insert(QStringLiteral("DesignedCapacity"), 42581u);
        row.insert(QStringLiteral("ManufactureName"), QStringLiteral("ACME Corp"));

        // (a) First success: cache filled, snapshot populated.
        BatterySnapshot s1;
        reader.applyStaticDataResult(s1, true, { row }, QString());
        QCOMPARE(reader.staticCache().size(), 1);
        QCOMPARE(s1.batteries.size(), 1);
        QCOMPARE(s1.batteries.first().manufacturer, QStringLiteral("ACME Corp"));
        QVERIFY(s1.unavailable.isEmpty());

        // (b) Transient 0x80041001-style failure: cache reused, identity
        // survives, NO unavailable entry (the data is known).
        BatterySnapshot s2;
        reader.applyStaticDataResult(s2, false, {}, QStringLiteral("0x80041001"));
        QCOMPARE(s2.batteries.size(), 1);
        QCOMPARE(s2.batteries.first().designedCapacitymWh.value_or(0), 42581u);
        QVERIFY(s2.unavailable.isEmpty());

        // (c) Fresh reader failing with no cache: honest unavailable entry.
        BatteryReader cold;
        BatterySnapshot s3;
        cold.applyStaticDataResult(s3, false, {}, QStringLiteral("0x80041001"));
        QVERIFY(s3.batteries.isEmpty());
        QCOMPARE(s3.unavailable.size(), 1);
        QVERIFY(s3.unavailable.first().contains(QStringLiteral("0x80041001")));

        // (d) Success with zero instances (desktop): no cache, no complaint.
        BatteryReader desktop;
        BatterySnapshot s4;
        desktop.applyStaticDataResult(s4, true, {}, QString());
        QVERIFY(s4.batteries.isEmpty());
        QVERIFY(s4.unavailable.isEmpty());
        QVERIFY(desktop.staticCache().isEmpty());

        // (e) A later success refreshes the cache (firmware re-learn).
        QVariantMap row2 = row;
        row2.insert(QStringLiteral("DesignedCapacity"), 42600u);
        BatterySnapshot s5;
        reader.applyStaticDataResult(s5, true, { row2 }, QString());
        QCOMPARE(reader.staticCache().first()
                     .value(QStringLiteral("DesignedCapacity")).toUInt(), 42600u);
    }

    void cimDatetimeFuzz_data()
    {
        QTest::addColumn<QString>("input");
        // Structured corpus.
        QTest::newRow("empty") << QString();
        QTest::newRow("short") << QStringLiteral("2025");
        QTest::newRow("wildcards") << QStringLiteral("**************.**********");
        QTest::newRow("valid") << QStringLiteral("20250821000000.000000+000");
        QTest::newRow("no-suffix") << QStringLiteral("20250821");
        QTest::newRow("month-13") << QStringLiteral("20251340000000.000000+000");
        QTest::newRow("day-00") << QStringLiteral("20250800000000.000000+000");
        QTest::newRow("year-1899") << QStringLiteral("18990101000000.000000+000");
        QTest::newRow("year-2101") << QStringLiteral("21010101000000.000000+000");
        QTest::newRow("negative") << QStringLiteral("-0250821000000.000000+000");
        QTest::newRow("spaces") << QStringLiteral("  20250821xxxx");
        // Random garbage strings.
        Gen g(0xF122ADE6u);
        for (int i = 0; i < 200; ++i) {
            QString s;
            const int n = int(g.u32Below(40));
            const QString alphabet = QStringLiteral("0123456789*.+-abcXYZ \t");
            for (int k = 0; k < n; ++k)
                s.append(alphabet.at(int(g.u32Below(quint32(alphabet.size())))));
            QTest::addRow("rand%03d", i) << s;
        }
    }
    void cimDatetimeFuzz()
    {
        QFETCH(QString, input);
        const QString out = BatteryReader::manufactureDateToIso(input);
        if (out.isEmpty())
            return; // rejection is always legal for garbage
        // Anything accepted must be a real, plausible ISO date.
        const auto m = QRegularExpression(QStringLiteral("^(\\d{4})-(\\d{2})-(\\d{2})$"))
                           .match(out);
        QVERIFY2(m.hasMatch(), qPrintable(input + " -> " + out));
        const QDate d(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
        QVERIFY(d.isValid());
        QVERIFY(d.year() >= 1990 && d.year() <= 2100);
    }

    void fourCCFuzz_data()
    {
        QTest::addColumn<quint32>("code");
        for (quint32 v : u32Boundaries())
            QTest::addRow("pool=%u", v) << v;
        Gen g(0xF122ADE7u);
        for (int i = 0; i < 300; ++i)
            QTest::addRow("rand%03d", i) << g.u32();
    }
    void fourCCFuzz()
    {
        QFETCH(quint32, code);
        const QString out = BatteryReader::chemistryFourCCToString(code);
        // Contract: empty, a known friendly name, or the printable ASCII
        // passthrough (<= 4 chars). Never control characters.
        QVERIFY(out.size() <= 24);
        for (const QChar &c : out) {
            QVERIFY2(c.isPrint(), qPrintable(QStringLiteral("code=%1 out=%2")
                                                 .arg(code).arg(out)));
        }
    }

    void chemistryTokenFuzz_data()
    {
        QTest::addColumn<QString>("token");
        Gen g(0xF122ADE8u);
        for (int i = 0; i < 120; ++i) {
            QString s;
            const int n = int(g.u32Below(16));
            for (int k = 0; k < n; ++k)
                s.append(QChar(ushort(g.u32In(1, 0x2FFF))));
            QTest::addRow("tok%03d", i) << s;
        }
    }
    void chemistryTokenFuzz()
    {
        QFETCH(QString, token);
        const QString out = BatteryReader::normalizeChemistryToken(token);
        // Unknown tokens pass through trimmed; known ones map to a name.
        // Either way: bounded, never invents content longer than the input
        // unless it is one of the fixed friendly names.
        static const QStringList known = {
            QStringLiteral("Lithium-ion"), QStringLiteral("Lithium Polymer"),
            QStringLiteral("Lead Acid"), QStringLiteral("Nickel Cadmium"),
            QStringLiteral("Nickel Metal Hydride"), QStringLiteral("Nickel Zinc"),
            QStringLiteral("Rechargeable Alkaline")
        };
        QVERIFY(out == token.trimmed() || known.contains(out) || out.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestFuzzReader)
#include "test_fuzz_reader.moc"
