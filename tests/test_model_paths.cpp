#include <QtTest/QtTest>

#include "SyntheticData.h"
#include "app/AlertManager.h"
#include "app/BatteryModel.h"

#include <QTemporaryDir>

using namespace faraday;

// Targeted coverage of BatteryModel paths the coverage map showed dark:
// time-estimate branches, exports (success + failure), setters/clamps,
// status-text branches, unit formatting, charge-cap fallback.
class TestModelPaths : public QObject
{
    Q_OBJECT
private slots:
    void timeEstimateChargingPath()
    {
        BatteryModel model;
        BatterySnapshot snap = synthetic::snapshot({ 10.0 }, 60.0, true);
        snap.batteries[0].charging = true;
        snap.batteries[0].discharging = false;
        snap.batteries[0].chargeRatemW = 9000;
        model.applySnapshot(snap);
        const QString text = model.timeEstimateText();
        QVERIFY2(text.contains(QStringLiteral("full charge")), qPrintable(text));
    }

    void timeEstimateTrendPreferredWhenDischarging()
    {
        BatteryModel model;
        const QDateTime t0(QDate(2026, 7, 12), QTime(9, 0));
        // Three discharging samples with a clear linear energy decline.
        for (int i = 0; i < 4; ++i) {
            BatterySnapshot snap = synthetic::snapshot({ 10.0 }, 60.0);
            snap.timestamp = t0.addSecs(i * 60);
            snap.batteries[0].remainingCapacitymWh = 27000 - i * 500;
            model.applySnapshot(snap);
        }
        const QString text = model.timeEstimateText();
        QVERIFY2(text.contains(QStringLiteral("trend")), qPrintable(text));

        // Instantaneous fallback: wipe the series, single sample only.
        model.resetSessionOrigin();
        BatterySnapshot one = synthetic::snapshot({ 10.0 }, 60.0);
        one.timestamp = t0.addSecs(3600);
        model.applySnapshot(one);
        const QString instant = model.timeEstimateText();
        QVERIFY2(instant.contains(QStringLiteral("remaining"))
                     && !instant.contains(QStringLiteral("trend")),
                 qPrintable(instant));
    }

    void timeEstimateRuntimeFallback()
    {
        BatteryModel model;
        BatterySnapshot snap = synthetic::snapshot({ 10.0 }, 60.0);
        snap.batteries[0].dischargeRatemW = 0;         // no instantaneous rate
        snap.batteries[0].discharging = true;
        snap.batteries[0].estimatedRuntimeSec = 5400;  // firmware's own figure
        model.applySnapshot(snap);
        const QString text = model.timeEstimateText();
        QVERIFY2(text.contains(QStringLiteral("1 h 30 min")), qPrintable(text));
    }

    void statusTextBranches()
    {
        BatteryModel model;
        // Fully charged: on AC, not charging, >= 99.5 %.
        BatterySnapshot full = synthetic::snapshot({ 0.0 }, 100.0, true);
        full.batteries[0].charging = false;
        full.batteries[0].discharging = false;
        model.applySnapshot(full);
        QCOMPARE(model.statusText(), QStringLiteral("Fully charged"));

        // On AC below full.
        BatterySnapshot ac = synthetic::snapshot({ 0.0 }, 80.0, true);
        ac.batteries[0].charging = false;
        ac.batteries[0].discharging = false;
        model.applySnapshot(ac);
        QCOMPARE(model.statusText(), QStringLiteral("On AC power"));

        // Win32 enum fallback when the ACPI flags are absent.
        BatterySnapshot enumOnly = synthetic::snapshot({ 0.0 }, 50.0);
        enumOnly.batteries[0].charging.reset();
        enumOnly.batteries[0].discharging.reset();
        enumOnly.batteries[0].powerOnline.reset();
        enumOnly.batteries[0].win32Status = 11; // Partially charged
        model.applySnapshot(enumOnly);
        QCOMPARE(model.statusText(), QStringLiteral("Partially charged"));

        // Nothing at all -> Idle.
        BatterySnapshot bare = synthetic::snapshot({ 0.0 }, 50.0);
        bare.batteries[0].charging.reset();
        bare.batteries[0].discharging.reset();
        bare.batteries[0].powerOnline.reset();
        model.applySnapshot(bare);
        QCOMPARE(model.statusText(), QStringLiteral("Idle"));
    }

    void exportsSucceedIntoOverrideDir()
    {
        QTemporaryDir data, exports;
        QVERIFY(data.isValid() && exports.isValid());
        BatteryModel model;
        model.initialize(data.path());
        model.setExportDirOverride(exports.path());
        model.applySnapshot(synthetic::snapshot({ 10.0 }));

        const QString csv = model.exportSamplesCsv(30);
        QVERIFY2(!csv.isEmpty(), qPrintable(model.lastExportError()));
        QVERIFY(QFileInfo::exists(csv));
        QVERIFY(csv.endsWith(QStringLiteral(".csv")));

        const QString json = model.exportSamplesJson(30);
        QVERIFY(!json.isEmpty());
        QVERIFY(QFileInfo::exists(json));

        const QString html = model.exportHtmlReport();
        QVERIFY2(!html.isEmpty(), qPrintable(model.lastExportError()));
        QVERIFY(QFileInfo::exists(html));
        // The HTML report must be self-contained (no scripts, no remote).
        QFile f(html);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray body = f.readAll();
        QVERIFY(!body.contains("<script"));
        QVERIFY(!body.contains("https://"));
        QVERIFY(model.lastExportError().isEmpty());
    }

    void exportsFailCleanlyIntoInvalidDir()
    {
        QTemporaryDir data;
        BatteryModel model;
        model.initialize(data.path());
        model.setExportDirOverride(QStringLiteral("ZZ:\\no\\such\\volume"));
        model.applySnapshot(synthetic::snapshot({ 10.0 }));

        QVERIFY(model.exportSamplesCsv(30).isEmpty());
        QVERIFY(!model.lastExportError().isEmpty());
        QVERIFY(model.exportSamplesJson(30).isEmpty());
        QVERIFY(!model.lastExportError().isEmpty());
        QVERIFY(model.exportHtmlReport().isEmpty());
        QVERIFY(!model.lastExportError().isEmpty());
    }

    void settersClampAndPersist()
    {
        QTemporaryDir data;
        BatteryModel model;
        model.initialize(data.path());

        model.setSampleIntervalSec(0);      // below the clamp
        QCOMPARE(model.sampleIntervalSec(), 1);
        model.setSampleIntervalSec(999999); // above the clamp
        QCOMPARE(model.sampleIntervalSec(), 3600);
        model.setSampleIntervalSec(3600);   // same value: early-return path
        QCOMPARE(model.sampleIntervalSec(), 3600);

        model.setTheme(QStringLiteral("light"));
        QCOMPARE(model.theme(), QStringLiteral("light"));
        model.setTheme(QStringLiteral("light")); // no-op branch
        QCOMPARE(model.theme(), QStringLiteral("light"));

        model.setSetting(QStringLiteral("sampleIntervalSec"), 45);
        QCOMPARE(model.sampleIntervalSec(), 45);
        QCOMPARE(model.settingValue(QStringLiteral("sampleIntervalSec")).toInt(), 45);

        // initialize() is idempotent — a second call must be a no-op.
        model.initialize(QStringLiteral("C:\\would\\be\\wrong"));
        QCOMPARE(model.dataDirPath(), data.path());
    }

    void unitFormattingBranches()
    {
        QTemporaryDir data;
        BatteryModel model;
        model.initialize(data.path());

        model.setSetting(QStringLiteral("temperatureUnit"), QStringLiteral("F"));
        QCOMPARE(model.formatTemperature(100.0), QStringLiteral("212.0 °F"));
        model.setSetting(QStringLiteral("temperatureUnit"), QStringLiteral("C"));
        QCOMPARE(model.formatTemperature(25.05), QStringLiteral("25.1 °C"));

        model.setSetting(QStringLiteral("energyUnit"), QStringLiteral("Wh"));
        QCOMPARE(model.formatEnergy(42581), QStringLiteral("42.6 Wh"));
        model.setSetting(QStringLiteral("energyUnit"), QStringLiteral("mWh"));
        QCOMPARE(model.formatEnergy(42581), QStringLiteral("42581 mWh"));

        QCOMPARE(model.formatDuration(-5), QString());
        QCOMPARE(model.formatDuration(59), QStringLiteral("0 min"));
        QCOMPARE(model.formatDuration(3660), QStringLiteral("1 h 1 min"));
    }

    void chargeCapToggleOnUnsupportedHardware()
    {
        BatteryModel model;
        // This machine (HP) exposes no vendor cap: the toggle must return an
        // honest error string, never pretend success, never elevate.
        const QString result = model.tryToggleChargeCap(true);
        QVERIFY(!result.isEmpty());
        QVERIFY(result != QStringLiteral("Charge cap updated."));
    }

    void acBatteryTransitionCycle()
    {
        // Force-simulated AC<->battery transitions (physically unpluggable
        // from software): discharge -> plug in -> charge -> full -> unplug.
        // The model must track every flip; the alert input must follow.
        BatteryModel model;
        const QDateTime t0(QDate(2026, 7, 12), QTime(9, 0));

        BatterySnapshot onBattery = synthetic::snapshot({ 10.0 }, 60.0, false);
        onBattery.timestamp = t0;
        model.applySnapshot(onBattery);
        QVERIFY(!model.onAcPower());
        QVERIFY(!model.charging());
        QVERIFY(model.currentAlertInput().discharging);

        BatterySnapshot plugged = synthetic::snapshot({ 10.0 }, 60.0, true);
        plugged.timestamp = t0.addSecs(30);
        plugged.batteries[0].charging = true;
        plugged.batteries[0].discharging = false;
        plugged.batteries[0].chargeRatemW = 20000;
        model.applySnapshot(plugged);
        QVERIFY(model.onAcPower());
        QVERIFY(model.charging());
        QCOMPARE(model.statusText(), QStringLiteral("Charging"));
        QVERIFY(model.currentAlertInput().onAcPower);

        BatterySnapshot full = synthetic::snapshot({ 10.0 }, 100.0, true);
        full.timestamp = t0.addSecs(60);
        full.batteries[0].charging = false;
        full.batteries[0].discharging = false;
        model.applySnapshot(full);
        QCOMPARE(model.statusText(), QStringLiteral("Fully charged"));

        BatterySnapshot unplugged = synthetic::snapshot({ 10.0 }, 99.0, false);
        unplugged.timestamp = t0.addSecs(90);
        model.applySnapshot(unplugged);
        QVERIFY(!model.onAcPower());
        QCOMPARE(model.statusText(), QStringLiteral("Discharging"));
        // The live series spans the whole cycle with finite points.
        QCOMPARE(model.liveSeries().size(), 4);
    }

    void usageLogAndDegradationGuards()
    {
        BatteryModel model;
        model.applySnapshot(synthetic::snapshot({ 10.0 }));
        // No report at all: empty log, invalid degradation — no crash.
        QVERIFY(model.usageLog().isEmpty());
        QVERIFY(!model.degradationInfo().value(QStringLiteral("valid")).toBool());

        // Report with too little history: still invalid, still clean.
        model.applyReport(synthetic::decliningReport(2, 50000, 50000, 300));
        QVERIFY(!model.degradationInfo().value(QStringLiteral("valid")).toBool());

        // usageLog honors maxEntries and orders newest-first.
        PowercfgReportData report;
        report.ok = true;
        const QDateTime base(QDate(2026, 7, 1), QTime(0, 0));
        for (int i = 0; i < 10; ++i)
            report.usage << synthetic::usageEntry(base.addSecs(i * 3600),
                                                  QStringLiteral("Active"), false, 600, 100);
        model.applyReport(report);
        const QVariantList log = model.usageLog(4);
        QCOMPARE(log.size(), 4);
        QVERIFY(log.first().toMap().value(QStringLiteral("tsMs")).toLongLong()
                > log.last().toMap().value(QStringLiteral("tsMs")).toLongLong());
    }
};

QTEST_GUILESS_MAIN(TestModelPaths)
#include "test_model_paths.moc"
