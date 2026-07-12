#include <QtTest/QtTest>

#include "SyntheticData.h"
#include "acquisition/BatteryReader.h"
#include "app/AlertManager.h"
#include "app/BatteryModel.h"
#include "core/HealthVerdict.h"

#include <QTemporaryDir>

using namespace faraday;

// Regression tests for the defects found in field testing on the MSI
// laptop (worn battery, cycle-tracking-free firmware). The scenario below
// reproduces that machine: 33% wear, BatteryCycleCount reporting 0,
// voltage present, no thermal zone.
class TestFieldDefects : public QObject
{
    Q_OBJECT

    static BatterySnapshot msiLikeSnapshot()
    {
        BatterySnapshot snap = synthetic::snapshot({ 33.0 }, 78.2); // 67% health
        snap.batteries[0].cycleCount = std::nullopt; // class absent entirely
        return snap;
    }

    static bool anyDetailMentionsCycles(const QStringList &details)
    {
        for (const QString &d : details) {
            if (d.contains(QStringLiteral("cycle"), Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

private slots:
    // ---- F1: cycle-count sentinel ---------------------------------------

    void sanitizeCycleCountPolicy()
    {
        QVERIFY(!BatteryReader::sanitizeCycleCount(0).has_value()); // sentinel
        QCOMPARE(BatteryReader::sanitizeCycleCount(1).value_or(0), 1u);
        QCOMPARE(BatteryReader::sanitizeCycleCount(35).value_or(0), 35u);
        QCOMPARE(BatteryReader::sanitizeCycleCount(65535).value_or(0), 65535u);
    }

    void readerRowWithZeroCyclesYieldsAbsent()
    {
        BatterySnapshot snap;
        QVariantMap row;
        row.insert(QStringLiteral("InstanceName"), QStringLiteral("ACPI\\PNP0C0A\\1_0"));
        row.insert(QStringLiteral("CycleCount"), 0u);
        BatteryReader::applyRootWmiClassRows(snap, QByteArrayLiteral("BatteryCycleCount"),
                                             { row });
        QCOMPARE(snap.batteries.size(), 1);
        QVERIFY(!snap.batteries.first().cycleCount.has_value()); // never a phantom 0
    }

    void absentCyclesNeverRenderAsZero()
    {
        BatteryModel model;
        model.applySnapshot(msiLikeSnapshot());
        QCOMPARE(model.cycleCount(), -1); // UI renders "—", never "0"
    }

    void zeroCyclesViaWmiSentinelNeverRenderAsZero()
    {
        BatteryModel model;
        BatterySnapshot snap = msiLikeSnapshot();
        snap.batteries[0].cycleCount = 0; // hostile: sentinel injected directly
        model.applySnapshot(snap);
        QCOMPARE(model.cycleCount(), -1);
    }

    void zeroCyclesViaPowercfgSentinelNeverRenderAsZero()
    {
        BatteryModel model;
        model.applySnapshot(msiLikeSnapshot());
        PowercfgReportData report;
        report.ok = true;
        PowercfgReportData::BatteryInfo info;
        info.designCapacitymWh = 52007;
        info.cycleCount = 0; // powercfg also reports 0 on untracked firmware
        report.batteries.append(info);
        model.applyReport(report);
        QCOMPARE(model.cycleCount(), -1);
    }

    void genuineCycleCountStillWorks()
    {
        BatteryModel model;
        BatterySnapshot snap = msiLikeSnapshot();
        snap.batteries[0].cycleCount = 35;
        model.applySnapshot(snap);
        QCOMPARE(model.cycleCount(), 35);
    }

    void verdictSaysNothingAboutCyclesWhenAbsent()
    {
        // The exact MSI failure: 33% wear + absent cycles must produce a
        // verdict with ZERO cycle-derived claims — especially not
        // "early in its life".
        BatteryModel model;
        model.applySnapshot(msiLikeSnapshot());
        const QStringList details = model.verdictDetails();
        QVERIFY2(!anyDetailMentionsCycles(details), qPrintable(details.join(QLatin1Char('|'))));
        for (const QString &d : details)
            QVERIFY(!d.contains(QStringLiteral("early in its life")));

        // Defense in depth: the verdict engine itself suppresses zero.
        VerdictInput direct;
        direct.healthPercent = 67.0;
        direct.cycleCount = 0u;
        const Verdict v = HealthVerdict::generate(direct);
        QVERIFY(!anyDetailMentionsCycles(v.details));
    }

    void verdictCannotContradictItself()
    {
        // Low MEASURED cycles + real wear is calendar aging: the verdict
        // must not claim "early in its life" against a 33%-worn pack.
        VerdictInput worn;
        worn.healthPercent = 67.0;
        worn.cycleCount = 40u;
        const Verdict v = HealthVerdict::generate(worn);
        bool foundCalendarLine = false;
        for (const QString &d : v.details) {
            QVERIFY2(!d.contains(QStringLiteral("early in its life")), qPrintable(d));
            if (d.contains(QStringLiteral("calendar aging")))
                foundCalendarLine = true;
        }
        QVERIFY(foundCalendarLine);

        // A genuinely healthy low-cycle pack keeps the early-life wording.
        VerdictInput fresh;
        fresh.healthPercent = 99.0;
        fresh.cycleCount = 40u;
        bool foundEarly = false;
        for (const QString &d : HealthVerdict::generate(fresh).details)
            if (d.contains(QStringLiteral("early in its life")))
                foundEarly = true;
        QVERIFY(foundEarly);

        // Sweep: across all health x cycle-presence combos, "early in its
        // life" may only ever appear when health >= 80 (or unmeasured).
        for (int health = 0; health <= 100; health += 5) {
            for (const int cycles : { -1, 0, 40, 200, 700 }) {
                VerdictInput in;
                in.healthPercent = double(health);
                if (cycles >= 0)
                    in.cycleCount = quint32(cycles);
                for (const QString &d : HealthVerdict::generate(in).details) {
                    if (d.contains(QStringLiteral("early in its life"))) {
                        QVERIFY2(health >= 80 && cycles > 0,
                                 qPrintable(QStringLiteral("health=%1 cycles=%2: %3")
                                                .arg(health).arg(cycles).arg(d)));
                    }
                }
            }
        }
    }

    void htmlReportOmitsCyclesWhenAbsent()
    {
        QTemporaryDir data, exports;
        BatteryModel model;
        model.initialize(data.path());
        model.setExportDirOverride(exports.path());
        model.applySnapshot(msiLikeSnapshot());
        const QString path = model.exportHtmlReport();
        QVERIFY(!path.isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString body = QString::fromUtf8(f.readAll());
        QVERIFY(!body.contains(QStringLiteral(">CYCLES<"))); // metric omitted
        QVERIFY(!body.contains(QStringLiteral("early in its life")));
    }

    void historyChartDropsZeroCyclePoints()
    {
        BatteryModel model;
        model.applySnapshot(msiLikeSnapshot());
        PowercfgReportData report;
        report.ok = true;
        for (int i = 0; i < 4; ++i) {
            PowercfgReportData::HistoryEntry h;
            h.start = QDateTime(QDate(2026, 1, 5).addDays(i * 7), QTime(0, 0));
            h.end = h.start.addDays(7);
            h.fullChargeCapacitymWh = 40000 - i * 200;
            h.cycleCount = 0; // untracked firmware writes zeros into history
            report.history.append(h);
        }
        model.applyReport(report);
        for (const QVariant &v : model.capacityHistoryList()) {
            const QVariant cycles = v.toMap().value(QStringLiteral("cycleCount"));
            QVERIFY(!cycles.isValid() || cycles.isNull()); // no phantom zeros plotted
        }
    }

    // ---- F1 audit: voltage sentinel --------------------------------------

    void zeroVoltageIsAbsentNotZeroVolts()
    {
        BatteryModel model;
        BatterySnapshot snap = msiLikeSnapshot();
        snap.batteries[0].voltagemV = 0; // firmware stub: pack cannot be at 0 V
        model.applySnapshot(snap);
        QCOMPARE(model.voltageV(), -1.0);              // UI renders "—"
        QVERIFY(!model.currentAlertInput().voltageV.has_value()); // no false alert
    }

    void genuineVoltageStillWorks()
    {
        BatteryModel model;
        model.applySnapshot(msiLikeSnapshot()); // synthetic pack: 11.4 V
        QVERIFY(qAbs(model.voltageV() - 11.4) < 0.001);
    }

    // ---- F3: ACPI identifiers are not product names ----------------------

    void acpiIdentifierNotShownAsProductName()
    {
        BatteryModel model;
        BatterySnapshot snap = msiLikeSnapshot();
        snap.batteries[0].name = QStringLiteral("BIF0_9"); // raw ACPI object id
        snap.batteries[0].uniqueId = QStringLiteral("ACMEBIF0_9");
        model.applySnapshot(snap);

        QHash<QString, QVariantMap> byField;
        for (const QVariant &v : model.staticInfo()) {
            const QVariantMap m = v.toMap();
            byField.insert(m.value(QStringLiteral("field")).toString(), m);
        }
        // Device-name row: honest "no product name", not the raw identifier.
        const QVariantMap name = byField.value(QStringLiteral("Device name"));
        QVERIFY(!name.value(QStringLiteral("available")).toBool());
        // Unique ID no longer sits in the identity panel at all...
        QVERIFY(!byField.contains(QStringLiteral("Unique ID")));
        // ...but both raw values remain reachable in the Advanced drawer.
        bool rawNameInDrawer = false, rawUidInDrawer = false;
        for (const QVariant &v : model.rawStreams()) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("value")).toString() == QLatin1String("BIF0_9"))
                rawNameInDrawer = true;
            if (m.value(QStringLiteral("value")).toString() == QLatin1String("ACMEBIF0_9"))
                rawUidInDrawer = true;
        }
        QVERIFY(rawNameInDrawer);
        QVERIFY(rawUidInDrawer);
    }

    void humanReadableNameStillShown()
    {
        BatteryModel model;
        BatterySnapshot snap = msiLikeSnapshot();
        snap.batteries[0].name = QStringLiteral("Primary"); // HP-style, readable
        model.applySnapshot(snap);
        QHash<QString, QVariantMap> byField;
        for (const QVariant &v : model.staticInfo()) {
            const QVariantMap m = v.toMap();
            byField.insert(m.value(QStringLiteral("field")).toString(), m);
        }
        const QVariantMap name = byField.value(QStringLiteral("Device name"));
        QVERIFY(name.value(QStringLiteral("available")).toBool());
        QCOMPARE(name.value(QStringLiteral("value")).toString(), QStringLiteral("Primary"));
    }

    void looksLikeAcpiIdentifierHeuristic()
    {
        QVERIFY(BatteryModel::looksLikeAcpiIdentifier(QStringLiteral("BIF0_9")));
        QVERIFY(BatteryModel::looksLikeAcpiIdentifier(QStringLiteral("ACMEBIF0_9")));
        QVERIFY(BatteryModel::looksLikeAcpiIdentifier(QStringLiteral("BAT1")));
        QVERIFY(!BatteryModel::looksLikeAcpiIdentifier(QStringLiteral("Primary")));
        QVERIFY(!BatteryModel::looksLikeAcpiIdentifier(QStringLiteral("Smart Battery")));
        // A digit-first, underscore-free part number (Lenovo FRU style) is
        // real information and must stay visible.
        QVERIFY(!BatteryModel::looksLikeAcpiIdentifier(QStringLiteral("45N1127")));
        QVERIFY(!BatteryModel::looksLikeAcpiIdentifier(QString()));
    }

    // ---- Field regression guard: what worked on the MSI must keep working

    void msiScenarioEndToEnd()
    {
        BatteryModel model;
        model.applySnapshot(msiLikeSnapshot());
        QVERIFY(qAbs(model.healthPercent() - 67.0) < 0.01);
        QVERIFY(qAbs(model.wearPercent() - 33.0) < 0.01);
        QCOMPARE(model.gradeName(), QStringLiteral("Fair"));
        QCOMPARE(model.gradeColor(), QStringLiteral("#d29922")); // amber ring
        QCOMPARE(model.cycleCount(), -1);
        QVERIFY(!model.temperatureKnown()); // no zone: "—", alert unavailable
        QVERIFY(!model.temperatureAlertAvailable());
    }
};

QTEST_GUILESS_MAIN(TestFieldDefects)
#include "test_field_defects.moc"
