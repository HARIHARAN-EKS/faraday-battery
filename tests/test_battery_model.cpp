#include <QtTest/QtTest>

#include "app/BatteryModel.h"

#include <QSignalSpy>

using namespace faraday;

namespace {

BatterySnapshot chargingSnapshot()
{
    BatterySnapshot snap;
    snap.timestamp = QDateTime::currentDateTime();
    snap.batteryPresent = true;
    BatteryDevice dev;
    dev.instanceName = QStringLiteral("ACPI\\PNP0C0A\\1_0");
    dev.designedCapacitymWh = 42401;
    dev.fullChargedCapacitymWh = 42401;
    dev.remainingCapacitymWh = 32288;
    dev.chargeRatemW = 31664;
    dev.dischargeRatemW = 0;
    dev.voltagemV = 12686;
    dev.powerOnline = true;
    dev.charging = true;
    dev.discharging = false;
    dev.cycleCount = 35;
    dev.estimatedChargeRemainingPct = 76;
    snap.batteries.append(dev);
    snap.temperatureC = 33.7;
    return snap;
}

} // namespace

class TestBatteryModel : public QObject
{
    Q_OBJECT
private slots:
    void sentinelsBeforeData()
    {
        BatteryModel model;
        QVERIFY(!model.ready());
        QVERIFY(!model.batteryPresent());
        QCOMPARE(model.chargePercent(), -1.0);
        QCOMPARE(model.healthPercent(), -1.0);
        QCOMPARE(model.cycleCount(), -1);
        QVERIFY(!model.temperatureKnown());
        QVERIFY(!model.powerKnown());
        QCOMPARE(model.designCapacitymWh(), qint64(-1));
        QVERIFY(!model.statusText().isEmpty()); // "Reading sensors…"
    }

    void snapshotDrivesProperties()
    {
        BatteryModel model;
        QSignalSpy spy(&model, &BatteryModel::snapshotChanged);
        model.applySnapshot(chargingSnapshot());

        QCOMPARE(spy.count(), 1);
        QVERIFY(model.ready());
        QVERIFY(model.batteryPresent());
        QVERIFY(qAbs(model.chargePercent() - 76.149) < 0.01);
        QCOMPARE(model.healthPercent(), 100.0);
        QCOMPARE(model.wearPercent(), 0.0);
        QCOMPARE(model.cycleCount(), 35);
        QVERIFY(model.temperatureKnown());
        QCOMPARE(model.temperatureC(), 33.7);
        QVERIFY(model.powerKnown());
        QVERIFY(qAbs(model.netPowerW() - 31.664) < 1e-9);
        QVERIFY(qAbs(model.voltageV() - 12.686) < 1e-9);
        QCOMPARE(model.statusText(), QStringLiteral("Charging"));
        QVERIFY(model.onAcPower());
        QVERIFY(model.charging());
        QVERIFY(model.timeEstimateText().contains(QStringLiteral("full")));
        QCOMPARE(model.gradeName(), QStringLiteral("Excellent"));
        QCOMPARE(model.gradeColor(), QStringLiteral("#3fb950"));
        QVERIFY(!model.verdictHeadline().isEmpty());
        QCOMPARE(model.batteries().size(), 1);
        QVERIFY(model.rawStreams().size() > 5);
    }

    void noBatterySnapshot()
    {
        BatteryModel model;
        BatterySnapshot empty;
        empty.timestamp = QDateTime::currentDateTime();
        empty.batteryPresent = false;
        model.applySnapshot(empty);
        QVERIFY(model.ready());
        QVERIFY(!model.batteryPresent());
        QCOMPARE(model.statusText(), QStringLiteral("No battery detected"));
        QCOMPARE(model.chargePercent(), -1.0);
    }

    void reportFillsGaps()
    {
        // Reader gives no design capacity / cycles (BatteryStaticData blocked);
        // the powercfg report must backfill both.
        BatteryModel model;
        BatterySnapshot snap = chargingSnapshot();
        snap.batteries[0].designedCapacitymWh.reset();
        snap.batteries[0].cycleCount.reset();
        model.applySnapshot(snap);
        QCOMPARE(model.designCapacitymWh(), qint64(-1));
        QCOMPARE(model.cycleCount(), -1);

        PowercfgReportData report;
        report.ok = true;
        PowercfgReportData::BatteryInfo info;
        info.designCapacitymWh = 42401;
        info.fullChargeCapacitymWh = 42401;
        info.cycleCount = 35;
        report.batteries.append(info);
        QSignalSpy reportSpy(&model, &BatteryModel::reportChanged);
        model.applyReport(report);

        QCOMPARE(reportSpy.count(), 1);
        QCOMPARE(model.designCapacitymWh(), qint64(42401));
        QCOMPARE(model.cycleCount(), 35);
        QCOMPARE(model.healthPercent(), 100.0);
    }

    void liveSeriesAccumulates()
    {
        BatteryModel model;
        QSignalSpy spy(&model, &BatteryModel::liveSeriesChanged);
        model.applySnapshot(chargingSnapshot());
        model.applySnapshot(chargingSnapshot());
        QCOMPARE(spy.count(), 2);
        QCOMPARE(model.liveSeries().size(), 2);
        const QVariantMap p = model.liveSeries().first().toMap();
        QVERIFY(p.contains(QStringLiteral("t")));
        QVERIFY(p.contains(QStringLiteral("percent")));
        QVERIFY(p.contains(QStringLiteral("powerW")));
        QVERIFY(p.value(QStringLiteral("charging")).toBool());

        model.resetSessionOrigin();
        QCOMPARE(model.liveSeries().size(), 0);
    }

    void multiBatteryAggregation()
    {
        BatteryModel model;
        BatterySnapshot snap = chargingSnapshot();
        BatteryDevice second = snap.batteries.first();
        second.instanceName = QStringLiteral("ACPI\\PNP0C0A\\2_0");
        second.remainingCapacitymWh = 10000;
        second.fullChargedCapacitymWh = 20000;
        second.designedCapacitymWh = 21000;
        snap.batteries.append(second);
        model.applySnapshot(snap);

        QCOMPARE(model.fullChargeCapacitymWh(), qint64(62401));
        QCOMPARE(model.designCapacitymWh(), qint64(63401));
        QCOMPARE(model.remainingCapacitymWh(), qint64(42288));
        // 42288 / 62401 = 67.77 %
        QVERIFY(qAbs(model.chargePercent() - 67.77) < 0.01);
        QCOMPARE(model.batteries().size(), 2);
    }

    void durationFormatting()
    {
        BatteryModel model;
        QCOMPARE(model.formatDuration(3660), QStringLiteral("1 h 1 min"));
        QCOMPARE(model.formatDuration(300), QStringLiteral("5 min"));
        QCOMPARE(model.formatDuration(-5), QString());
    }
};

QTEST_GUILESS_MAIN(TestBatteryModel)
#include "test_battery_model.moc"
