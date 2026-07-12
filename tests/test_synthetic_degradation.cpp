#include <QtTest/QtTest>

#include "SyntheticData.h"
#include "app/BatteryModel.h"
#include "core/Metrics.h"

using namespace faraday;

// Drives the whole model with fabricated degraded-battery states — paths a
// healthy reference machine (35 cycles, ~100 % health) never exercises live.
class TestSyntheticDegradation : public QObject
{
    Q_OBJECT
private slots:
    void wearTiersProduceCorrectMetrics()
    {
        struct Tier {
            double wear;
            double health;
            const char *grade;
            const char *color;
        };
        // 0/15/40/70 % wear per the verification spec, plus 30 % to pin the
        // amber (Fair) ring state explicitly.
        const Tier tiers[] = {
            { 0.0, 100.0, "Excellent", "#3fb950" },
            { 15.0, 85.0, "Good", "#7ee787" },
            { 30.0, 70.0, "Fair", "#d29922" },      // amber ring
            { 40.0, 60.0, "Worn", "#f0883e" },
            { 70.0, 30.0, "Replace soon", "#f85149" }, // red ring
        };

        for (const Tier &tier : tiers) {
            BatteryModel model;
            model.applySnapshot(synthetic::snapshot({ tier.wear }));

            QVERIFY2(qAbs(model.wearPercent() - tier.wear) < 0.01,
                     qPrintable(QStringLiteral("wear %1 -> %2")
                                    .arg(tier.wear).arg(model.wearPercent())));
            QVERIFY(qAbs(model.healthPercent() - tier.health) < 0.01);
            QCOMPARE(model.gradeName(), QLatin1String(tier.grade));
            QCOMPARE(model.gradeColor(), QLatin1String(tier.color));
        }
    }

    void verdictTextMatchesEachTier()
    {
        // The plain-language verdict must track the tier, never contradict it.
        BatteryModel excellent;
        excellent.applySnapshot(synthetic::snapshot({ 0.0 }));
        QVERIFY(excellent.verdictHeadline().contains(QStringLiteral("excellent"),
                                                     Qt::CaseInsensitive));

        BatteryModel fair;
        fair.applySnapshot(synthetic::snapshot({ 30.0 }));
        QVERIFY(fair.verdictHeadline().contains(QStringLiteral("noticeable wear"),
                                                Qt::CaseInsensitive));

        BatteryModel worn;
        worn.applySnapshot(synthetic::snapshot({ 40.0 }));
        QVERIFY(worn.verdictHeadline().contains(QStringLiteral("significantly worn"),
                                                Qt::CaseInsensitive));

        BatteryModel dead;
        dead.applySnapshot(synthetic::snapshot({ 70.0 }));
        QVERIFY(dead.verdictHeadline().contains(QStringLiteral("replaced"),
                                                Qt::CaseInsensitive));

        // Headlines must be distinct — a nonsense verdict engine that returns
        // one string for everything must fail here.
        QVERIFY(excellent.verdictHeadline() != worn.verdictHeadline());
        QVERIFY(worn.verdictHeadline() != dead.verdictHeadline());

        // Detail line must carry the capacity figure.
        QVERIFY(!dead.verdictDetails().isEmpty());
        QVERIFY(dead.verdictDetails().first().contains(QStringLiteral("30.0")));
    }

    void degradationCurveAndEol()
    {
        // 24 weeks of perfectly linear decline: 50000 mWh design, 300 mWh
        // lost per week => slope -300/7 = -42.857 mWh/day. The 80 % service
        // threshold (40000 mWh) is reached ~33 weeks after the series start,
        // safely in the future.
        BatteryModel model;
        model.applySnapshot(synthetic::snapshot({ 6.0 }));
        model.applyReport(synthetic::decliningReport(24, 50000, 50000, 300));

        const QVariantMap info = model.degradationInfo();
        QVERIFY(info.value(QStringLiteral("valid")).toBool());
        const double slope = info.value(QStringLiteral("slopeMWhPerDay")).toDouble();
        QVERIFY2(qAbs(slope + 300.0 / 7.0) < 0.5,
                 qPrintable(QStringLiteral("slope=%1").arg(slope)));
        QVERIFY(info.value(QStringLiteral("r2")).toDouble() > 0.999); // exact line
        QVERIFY(!info.value(QStringLiteral("eolDate")).toString().isEmpty());

        // The verdict picks the projection up too.
        const QStringList details = model.verdictDetails();
        bool mentionsDecline = false, mentionsEol = false;
        for (const QString &d : details) {
            if (d.contains(QStringLiteral("declining")))
                mentionsDecline = true;
            if (d.contains(QStringLiteral("service threshold")))
                mentionsEol = true;
        }
        QVERIFY(mentionsDecline);
        QVERIFY(mentionsEol);
    }

    void calibrationDriftDetected()
    {
        // Gauge says 80 %, measured energy says 70 % -> +10 % drift, which
        // must trip the calibration recommendation (threshold: |drift| > 5).
        BatteryModel model;
        BatterySnapshot snap;
        snap.timestamp = QDateTime::currentDateTime();
        snap.batteryPresent = true;
        snap.batteries.append(synthetic::pack(20.0, 70.0, false, 50000, 80));
        model.applySnapshot(snap);

        QVERIFY(model.calibrationDriftKnown());
        QVERIFY(qAbs(model.calibrationDrift() - 10.0) < 0.1);
        QVERIFY(model.calibrationRecommended());

        // A well-calibrated pack must NOT trigger it.
        BatteryModel healthy;
        healthy.applySnapshot(synthetic::snapshot({ 20.0 }, 70.0));
        QVERIFY(healthy.calibrationDriftKnown());
        QVERIFY(qAbs(healthy.calibrationDrift()) < 1.0);
        QVERIFY(!healthy.calibrationRecommended());
    }

    void perStateDrainFromSyntheticUsage()
    {
        // Two Active-on-battery hours draining 8000 mWh each -> 8 W expected
        // drain; the Suspend entry must not contaminate the Active figure.
        BatteryModel model;
        model.applySnapshot(synthetic::snapshot({ 10.0 }));

        PowercfgReportData report;
        report.ok = true;
        const QDateTime base(QDate(2026, 7, 1), QTime(9, 0));
        report.usage << synthetic::usageEntry(base, QStringLiteral("Active"),
                                              false, 3600, 8000)
                     << synthetic::usageEntry(base.addSecs(3600), QStringLiteral("Active"),
                                              false, 3600, 8000)
                     << synthetic::usageEntry(base.addSecs(7200), QStringLiteral("Suspend"),
                                              false, 3600, 100);
        model.applyReport(report);

        QVERIFY2(qAbs(model.expectedDrainW() - 8.0) < 0.01,
                 qPrintable(QStringLiteral("drain=%1").arg(model.expectedDrainW())));
        // FCC = 45000 mWh -> -100 * 8000 / 45000 = -17.78 %/h expected slope.
        QVERIFY(qAbs(model.expectedDischargeSlopePctPerHour() + 17.78) < 0.05);
    }

    void multiBatteryDegradedAggregation()
    {
        // Pack 0: 10 % wear (45000 FCC), pack 1: 50 % wear (25000 FCC) on a
        // 50000 design each. Aggregate health = 70000 / 100000 = 70 % (Fair).
        BatteryModel model;
        model.applySnapshot(synthetic::snapshot({ 10.0, 50.0 }));

        QCOMPARE(model.batteries().size(), 2);
        QVERIFY(qAbs(model.healthPercent() - 70.0) < 0.01);
        QCOMPARE(model.gradeName(), QStringLiteral("Fair"));
        QCOMPARE(model.fullChargeCapacitymWh(), qint64(70000));
        QCOMPARE(model.designCapacitymWh(), qint64(100000));

        // Static-info rows appear per pack, labeled.
        const QVariantList rows = model.staticInfo();
        QCOMPARE(rows.size(), 14); // 7 fields x 2 packs
        QCOMPARE(rows.first().toMap().value(QStringLiteral("packLabel")).toString(),
                 QStringLiteral("Pack 1"));
        QCOMPARE(rows.last().toMap().value(QStringLiteral("packLabel")).toString(),
                 QStringLiteral("Pack 2"));
    }

    void noBatteryAndGhostPaths()
    {
        // Empty snapshot (desktop) and zeroed ghost pack (VM) must both land
        // in the clean no-battery state with sentinel metrics — never a
        // bogus 0 % health or a scary verdict.
        BatteryModel desktop;
        BatterySnapshot empty;
        empty.timestamp = QDateTime::currentDateTime();
        desktop.applySnapshot(empty);
        QVERIFY(desktop.ready());
        QVERIFY(!desktop.batteryPresent());
        QCOMPARE(desktop.statusText(), QStringLiteral("No battery detected"));
        QCOMPARE(desktop.healthPercent(), -1.0);
        QCOMPARE(desktop.gradeName(), QStringLiteral("Unknown"));
        QCOMPARE(desktop.gradeColor(), QStringLiteral("#8b949e"));

        BatteryModel vm;
        vm.applySnapshot(synthetic::ghostSnapshot());
        QVERIFY(!vm.batteryPresent());
        QCOMPARE(vm.healthPercent(), -1.0);
        QCOMPARE(vm.wearPercent(), -1.0);
        QCOMPARE(vm.gradeName(), QStringLiteral("Unknown"));
    }

    void deepWearLiveSeriesStaysCoherent()
    {
        // A 70 %-worn pack discharging: live points must carry the computed
        // (energy-based) percentage, and the critical marker must reflect
        // abnormal draw, not wear.
        BatteryModel model;
        BatterySnapshot snap = synthetic::snapshot({ 70.0 }, 50.0);
        model.applySnapshot(snap);
        QCOMPARE(model.liveSeries().size(), 1);
        const QVariantMap p = model.liveSeries().first().toMap();
        QVERIFY(qAbs(p.value(QStringLiteral("percent")).toDouble() - 50.0) < 0.1);
        QVERIFY(!p.value(QStringLiteral("critical")).toBool()); // 8 W is normal

        // Same pack pulling 60 W off-AC: critical marker must fire.
        snap.batteries[0].dischargeRatemW = 60000;
        model.applySnapshot(snap);
        QVERIFY(model.liveSeries().last().toMap()
                    .value(QStringLiteral("critical")).toBool());
    }
};

QTEST_GUILESS_MAIN(TestSyntheticDegradation)
#include "test_synthetic_degradation.moc"
