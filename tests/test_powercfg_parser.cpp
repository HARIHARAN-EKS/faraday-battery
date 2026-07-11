#include <QtTest/QtTest>

#include "acquisition/PowercfgReport.h"
#include "PowercfgFixture.h"

using namespace faraday;

class TestPowercfgParser : public QObject
{
    Q_OBJECT
private slots:
    void parsesBatteryMetadata()
    {
        const PowercfgReportData data = PowercfgReport::parse(QByteArray(kSampleBatteryReportXml));
        QVERIFY2(data.ok, qPrintable(data.error));
        QCOMPARE(data.batteries.size(), 1);
        const auto &b = data.batteries.first();
        QCOMPARE(b.id, QStringLiteral("Primary"));
        QCOMPARE(b.manufacturer, QStringLiteral("Hewlett-Packard"));
        QCOMPARE(b.chemistry, QStringLiteral("LIon"));
        QCOMPARE(b.designCapacitymWh.value_or(-1), qint64(42401));
        QCOMPARE(b.fullChargeCapacitymWh.value_or(-1), qint64(40000));
        QCOMPARE(b.cycleCount.value_or(-1), qint64(35));
    }

    void parsesHistory()
    {
        const PowercfgReportData data = PowercfgReport::parse(QByteArray(kSampleBatteryReportXml));
        QVERIFY(data.ok);
        QCOMPARE(data.history.size(), 2);
        QCOMPARE(data.history.at(0).start.date(), QDate(2026, 6, 1));
        QCOMPARE(data.history.at(0).fullChargeCapacitymWh.value_or(-1), qint64(42413));
        QCOMPARE(data.history.at(1).fullChargeCapacitymWh.value_or(-1), qint64(42000));
        QCOMPARE(data.history.at(1).cycleCount.value_or(-1), qint64(34));
    }

    void parsesUsage()
    {
        const PowercfgReportData data = PowercfgReport::parse(QByteArray(kSampleBatteryReportXml));
        QVERIFY(data.ok);
        QCOMPARE(data.usage.size(), 2);
        QCOMPARE(data.usage.at(0).entryType, QStringLiteral("Active"));
        QVERIFY(data.usage.at(0).ac);
        QVERIFY(!data.usage.at(1).ac);
        QCOMPARE(data.usage.at(0).chargeCapacitymWh.value_or(-1), qint64(22455));
        QCOMPARE(data.usage.at(0).dischargemWh.value_or(0), qint64(-236));
        QCOMPARE(data.usage.at(1).timestamp,
                 QDateTime(QDate(2026, 7, 11), QTime(20, 32, 22)));
    }

    void rejectsEmptyInput()
    {
        const PowercfgReportData data = PowercfgReport::parse(QByteArray());
        QVERIFY(!data.ok);
        QVERIFY(!data.error.isEmpty());
    }

    void rejectsMalformedXml()
    {
        const PowercfgReportData data = PowercfgReport::parse(QByteArrayLiteral("<BatteryReport><Batt"));
        QVERIFY(!data.ok);
        QVERIFY(!data.error.isEmpty());
    }

    void rejectsForeignXml()
    {
        const PowercfgReportData data =
            PowercfgReport::parse(QByteArrayLiteral("<?xml version=\"1.0\"?><Other/>"));
        QVERIFY(!data.ok);
        QVERIFY(data.error.contains(QStringLiteral("BatteryReport")));
    }

    void handlesNoBatterySystem()
    {
        // Desktop machines still produce a report, just with no Battery nodes.
        const PowercfgReportData data = PowercfgReport::parse(QByteArrayLiteral(
            "<?xml version=\"1.0\"?><BatteryReport><Batteries></Batteries></BatteryReport>"));
        QVERIFY(data.ok);
        QVERIFY(data.batteries.isEmpty());
        QVERIFY(data.history.isEmpty());
        QVERIFY(data.usage.isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestPowercfgParser)
#include "test_powercfg_parser.moc"
