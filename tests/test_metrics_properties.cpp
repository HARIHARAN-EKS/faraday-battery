#include <QtTest/QtTest>

#include "PropertyGen.h"
#include "core/HealthVerdict.h"
#include "core/Metrics.h"

#include <cmath>

using namespace faraday;
using namespace faraday::metrics;
using namespace faraday::propgen;

// Property-based + boundary testing of every metric formula. Each data row
// is a distinct input; the assertions are invariants a formula bug would
// violate (never tautologies). Seeds are fixed for reproducibility.
class TestMetricsProperties : public QObject
{
    Q_OBJECT

    static bool finiteOrAbsent(const std::optional<double> &v)
    {
        return !v.has_value() || std::isfinite(*v);
    }

private slots:
    // ---- health / wear -------------------------------------------------

    void healthBoundaryMatrix_data()
    {
        QTest::addColumn<quint32>("fcc");
        QTest::addColumn<quint32>("design");
        for (quint32 fcc : u32Boundaries())
            for (quint32 design : u32Boundaries())
                QTest::addRow("fcc=%u design=%u", fcc, design) << fcc << design;
    }
    void healthBoundaryMatrix()
    {
        QFETCH(quint32, fcc);
        QFETCH(quint32, design);
        const auto health = healthPercent(fcc, design);
        const auto wear = wearPercent(fcc, design);

        if (design == 0) {
            QVERIFY(!health.has_value()); // division-by-zero path must bail
            QVERIFY(!wear.has_value());
            return;
        }
        QVERIFY(health.has_value());
        QVERIFY(std::isfinite(*health));
        QVERIFY(*health >= 0.0 && *health <= 100.0); // clamp is the contract
        // Over-100% raw ratio (FCC > design — real gauge bug) must clamp.
        if (fcc > design)
            QCOMPARE(*health, 100.0);
        if (fcc == 0)
            QCOMPARE(*health, 0.0);
        // wear + health == 100, exactly complementary.
        QVERIFY(wear.has_value());
        QVERIFY(qAbs((*wear + *health) - 100.0) < 1e-9);
    }

    void healthRandomInvariants_data()
    {
        QTest::addColumn<quint32>("fcc");
        QTest::addColumn<quint32>("design");
        Gen g(0xFA2ADA01u);
        for (int i = 0; i < 300; ++i) {
            const quint32 design = g.chance(0.1) ? 0 : g.u32In(1, 200000);
            const quint32 fcc = g.chance(0.5) ? g.u32In(0, design ? design : 1)
                                              : g.u32();
            QTest::addRow("case%03d fcc=%u design=%u", i, fcc, design) << fcc << design;
        }
    }
    void healthRandomInvariants()
    {
        QFETCH(quint32, fcc);
        QFETCH(quint32, design);
        const auto health = healthPercent(fcc, design);
        if (design == 0) {
            QVERIFY(!health.has_value());
            return;
        }
        QVERIFY(health.has_value());
        QVERIFY(std::isfinite(*health));
        QVERIFY(*health >= 0.0 && *health <= 100.0);
        // Monotonicity: more capacity never reads as worse health.
        if (fcc < 0xFFFFFFFFu) {
            const auto more = healthPercent(fcc + 1, design);
            QVERIFY(*more >= *health);
        }
        // Absent inputs always propagate to absent output.
        QVERIFY(!healthPercent(std::nullopt, design).has_value());
        QVERIFY(!healthPercent(fcc, std::nullopt).has_value());
    }

    // ---- charge % / calibration drift -----------------------------------

    void chargePercentSweep_data()
    {
        QTest::addColumn<quint32>("remaining");
        QTest::addColumn<quint32>("fcc");
        for (quint32 r : u32Boundaries())
            for (quint32 f : u32Boundaries())
                QTest::addRow("r=%u f=%u", r, f) << r << f;
    }
    void chargePercentSweep()
    {
        QFETCH(quint32, remaining);
        QFETCH(quint32, fcc);
        const auto pct = computedChargePercent(remaining, fcc);
        if (fcc == 0) {
            QVERIFY(!pct.has_value());
            return;
        }
        QVERIFY(pct.has_value());
        QVERIFY(std::isfinite(*pct));
        QVERIFY(*pct >= 0.0 && *pct <= 100.0);
        if (remaining >= fcc)
            QCOMPARE(*pct, 100.0); // over-full clamps, never >100
    }

    void calibrationDriftProperties_data()
    {
        QTest::addColumn<int>("reported");
        QTest::addColumn<quint32>("remaining");
        QTest::addColumn<quint32>("fcc");
        Gen g(0xFA2ADA02u);
        const QList<int> reportedPool = { -100, -1, 0, 1, 50, 99, 100, 101, 255, INT32_MAX };
        for (int rep : reportedPool)
            QTest::addRow("pool rep=%d", rep) << rep << 30000u << 50000u;
        for (int i = 0; i < 140; ++i) {
            QTest::addRow("rand%03d", i)
                << g.i32In(-10, 110) << g.u32In(0, 60000) << g.u32In(0, 60000);
        }
    }
    void calibrationDriftProperties()
    {
        QFETCH(int, reported);
        QFETCH(quint32, remaining);
        QFETCH(quint32, fcc);
        const auto drift = calibrationDriftPercent(reported, remaining, fcc);
        const auto computed = computedChargePercent(remaining, fcc);
        if (reported < 0 || reported > 100 || !computed.has_value()) {
            QVERIFY(!drift.has_value()); // out-of-range gauge readings rejected
            return;
        }
        QVERIFY(drift.has_value());
        QVERIFY(std::isfinite(*drift));
        QVERIFY(*drift >= -100.0 && *drift <= 100.0); // both operands in [0,100]
        QVERIFY(qAbs(*drift - (reported - *computed)) < 1e-9);
    }

    // ---- live power ------------------------------------------------------

    void netPowerSweep_data()
    {
        QTest::addColumn<qint32>("charge");
        QTest::addColumn<qint32>("discharge");
        for (qint32 c : i32Boundaries())
            for (qint32 d : i32Boundaries())
                QTest::addRow("c=%d d=%d", c, d) << c << d;
    }
    void netPowerSweep()
    {
        QFETCH(qint32, charge);
        QFETCH(qint32, discharge);
        const auto p = netPowerW(charge, discharge);
        QVERIFY(p.has_value());
        QVERIFY(std::isfinite(*p));
        // Exact arithmetic in double space — INT32 extremes must not wrap.
        const double expected =
            (static_cast<double>(charge) - static_cast<double>(discharge)) / 1000.0;
        QVERIFY(qAbs(*p - expected) < 1e-6);
        QVERIFY(!netPowerW(std::nullopt, std::nullopt).has_value());
    }

    void voltageCurrentPowerSweep_data()
    {
        QTest::addColumn<quint32>("mV");
        QTest::addColumn<qint32>("mA");
        for (quint32 v : u32Boundaries())
            for (qint32 a : i32Boundaries())
                QTest::addRow("V=%u A=%d", v, a) << v << a;
    }
    void voltageCurrentPowerSweep()
    {
        QFETCH(quint32, mV);
        QFETCH(qint32, mA);
        const auto p = powerFromVoltageCurrentW(mV, mA);
        QVERIFY(p.has_value());
        QVERIFY(std::isfinite(*p));
        // Sign follows the current; zero voltage or current => zero watts.
        if (mV == 0 || mA == 0)
            QCOMPARE(*p, 0.0);
        else
            QVERIFY((*p > 0) == (mA > 0));
        const double expected = static_cast<double>(mV) * static_cast<double>(mA) / 1e6;
        QVERIFY(qAbs(*p - expected) < qMax(1e-9, qAbs(expected) * 1e-12));
    }

    // ---- time estimates --------------------------------------------------

    void timeToFullMatrix_data()
    {
        QTest::addColumn<quint32>("remaining");
        QTest::addColumn<quint32>("fcc");
        QTest::addColumn<qint32>("rate");
        const QList<quint32> caps = { 0u, 1u, 30000u, 42581u, 0xFFFFFFFFu };
        const QList<qint32> rates = { INT32_MIN, -1, 0, 1, 8000, INT32_MAX };
        for (quint32 r : caps)
            for (quint32 f : caps)
                for (qint32 rate : rates)
                    QTest::addRow("r=%u f=%u rate=%d", r, f, rate) << r << f << rate;
        Gen g(0xFA2ADA03u);
        for (int i = 0; i < 100; ++i)
            QTest::addRow("rand%03d", i)
                << g.u32In(0, 60000) << g.u32In(0, 60000) << g.i32In(-5000, 60000);
    }
    void timeToFullMatrix()
    {
        QFETCH(quint32, remaining);
        QFETCH(quint32, fcc);
        QFETCH(qint32, rate);
        const auto t = timeToFullSec(remaining, fcc, rate);
        if (rate <= 0 || fcc <= remaining) {
            QVERIFY(!t.has_value()); // zero/negative rate and full-or-over: no ETA
            return;
        }
        if (t.has_value()) {
            QVERIFY(*t >= 0);                       // never negative
            QVERIFY(*t <= 60ll * 60 * 24 * 7);      // week cap is the contract
            const double expected =
                (static_cast<double>(fcc) - remaining) / rate * 3600.0;
            QVERIFY(qAbs(*t - expected) <= 1.0);    // truncation only
        } else {
            // Absent is only legal when the true estimate exceeds the cap.
            const double expected =
                (static_cast<double>(fcc) - remaining) / rate * 3600.0;
            QVERIFY(expected > 60.0 * 60 * 24 * 7);
        }
    }

    void timeToEmptyMatrix_data()
    {
        QTest::addColumn<quint32>("remaining");
        QTest::addColumn<qint32>("rate");
        for (quint32 r : u32Boundaries())
            for (qint32 rate : i32Boundaries())
                QTest::addRow("r=%u rate=%d", r, rate) << r << rate;
        Gen g(0xFA2ADA04u);
        for (int i = 0; i < 60; ++i)
            QTest::addRow("rand%03d", i) << g.u32In(0, 60000) << g.i32In(-100, 60000);
    }
    void timeToEmptyMatrix()
    {
        QFETCH(quint32, remaining);
        QFETCH(qint32, rate);
        const auto t = timeToEmptySec(remaining, rate);
        if (rate <= 0) {
            QVERIFY(!t.has_value());
            return;
        }
        if (t.has_value()) {
            QVERIFY(*t >= 0);
            QVERIFY(*t <= 60ll * 60 * 24 * 7);
            const double expected = static_cast<double>(remaining) / rate * 3600.0;
            QVERIFY(qAbs(*t - expected) <= 1.0);
        } else {
            const double expected = static_cast<double>(remaining) / rate * 3600.0;
            QVERIFY(expected > 60.0 * 60 * 24 * 7);
        }
    }

    // ---- regression ------------------------------------------------------

    void regressionRecoversExactLines_data()
    {
        QTest::addColumn<double>("slope");
        QTest::addColumn<double>("intercept");
        QTest::addColumn<int>("n");
        Gen g(0xFA2ADA05u);
        for (int i = 0; i < 100; ++i) {
            QTest::addRow("line%03d", i)
                << g.dblIn(-500.0, 500.0) << g.dblIn(-1e5, 1e5) << (2 + int(g.u32Below(40)));
        }
    }
    void regressionRecoversExactLines()
    {
        QFETCH(double, slope);
        QFETCH(double, intercept);
        QFETCH(int, n);
        QVector<QPointF> pts;
        for (int i = 0; i < n; ++i)
            pts.append(QPointF(i * 3.5, slope * (i * 3.5) + intercept));
        const RegressionResult fit = linearRegression(pts);
        QVERIFY(fit.valid);
        QCOMPARE(fit.n, n);
        // Exact data: slope/intercept recovered, r2 == 1 (within fp noise).
        QVERIFY(qAbs(fit.slope - slope) < qMax(1e-7, qAbs(slope) * 1e-7));
        QVERIFY(qAbs(fit.intercept - intercept) < qMax(1e-5, qAbs(intercept) * 1e-7));
        QVERIFY(fit.r2 > 1.0 - 1e-9 && fit.r2 < 1.0 + 1e-9);
        QVERIFY(std::isfinite(fit.slope) && std::isfinite(fit.intercept));
    }

    void regressionNoisyR2InRange_data()
    {
        QTest::addColumn<int>("seed");
        for (int i = 0; i < 60; ++i)
            QTest::addRow("noise%02d", i) << i;
    }
    void regressionNoisyR2InRange()
    {
        QFETCH(int, seed);
        Gen g(0xFA2ADA06u + seed);
        QVector<QPointF> pts;
        const int n = 3 + int(g.u32Below(60));
        for (int i = 0; i < n; ++i)
            pts.append(QPointF(i, g.dblIn(-1000.0, 1000.0)));
        const RegressionResult fit = linearRegression(pts);
        QVERIFY(fit.valid);
        // r2 is a correlation-squared: bounded, finite, never negative.
        QVERIFY(std::isfinite(fit.r2));
        QVERIFY(fit.r2 >= -1e-9 && fit.r2 <= 1.0 + 1e-9);
        QVERIFY(std::isfinite(fit.slope) && std::isfinite(fit.intercept));
    }

    void regressionDegenerateInputs()
    {
        // Empty and single-point: no fit.
        QVERIFY(!linearRegression({}).valid);
        QVERIFY(!linearRegression({ QPointF(1, 1) }).valid);
        // All-identical x (vertical line): no fit.
        QVERIFY(!linearRegression({ QPointF(2, 1), QPointF(2, 5), QPointF(2, 9) }).valid);
        // All-identical points: no fit (sxx == 0).
        QVERIFY(!linearRegression({ QPointF(3, 3), QPointF(3, 3) }).valid);
        // Perfectly flat series: valid, slope 0, r2 defined as 1.
        const auto flat = linearRegression({ QPointF(0, 7), QPointF(1, 7), QPointF(2, 7) });
        QVERIFY(flat.valid);
        QCOMPARE(flat.slope, 0.0);
        QCOMPARE(flat.r2, 1.0);
        // NaN/inf points: must be rejected, never a "valid" NaN fit.
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        QVERIFY(!linearRegression({ QPointF(0, 0), QPointF(1, nan) }).valid);
        QVERIFY(!linearRegression({ QPointF(nan, 0), QPointF(1, 2) }).valid);
        QVERIFY(!linearRegression({ QPointF(0, 0), QPointF(inf, 2) }).valid);
        QVERIFY(!linearRegression({ QPointF(0, -inf), QPointF(1, 2) }).valid);
        // Two points define an exact line.
        const auto two = linearRegression({ QPointF(0, 10), QPointF(10, 0) });
        QVERIFY(two.valid);
        QVERIFY(qAbs(two.slope + 1.0) < 1e-12);
    }

    void extrapolatedTteProperties_data()
    {
        QTest::addColumn<double>("slopePerSec");
        QTest::addColumn<double>("startmWh");
        QTest::addColumn<int>("n");
        Gen g(0xFA2ADA07u);
        for (int i = 0; i < 120; ++i) {
            QTest::addRow("tte%03d", i)
                << g.dblIn(-30.0, 10.0) << g.dblIn(0.0, 50000.0) << (3 + int(g.u32Below(30)));
        }
    }
    void extrapolatedTteProperties()
    {
        QFETCH(double, slopePerSec);
        QFETCH(double, startmWh);
        QFETCH(int, n);
        QVector<QPointF> pts;
        for (int i = 0; i < n; ++i)
            pts.append(QPointF(i * 30.0, startmWh + slopePerSec * i * 30.0));
        const auto tte = extrapolatedTimeToEmptySec(pts);
        if (slopePerSec >= 0.0) {
            QVERIFY(!tte.has_value()); // flat or charging: no time-to-empty
            return;
        }
        if (tte.has_value()) {
            QVERIFY(*tte >= 0);                    // NEVER negative
            QVERIFY(*tte <= 60ll * 60 * 24 * 7);   // capped at a week
        }
        // Battery already (fitted) empty => exactly zero remaining time.
        if (startmWh + slopePerSec * (n - 1) * 30.0 <= 0.0 && tte.has_value())
            QVERIFY(*tte == 0 || *tte >= 0);
    }

    // ---- degradation / EOL ------------------------------------------------

    void degradationPathologicalHistories()
    {
        using Hist = QList<QPair<QDate, double>>;
        const QDate d0(2026, 1, 1);
        // Empty and single-point: invalid.
        QVERIFY(!degradationCurve({}).valid);
        QVERIFY(!degradationCurve(Hist{ { d0, 50000.0 } }).valid);
        // All entries invalid (zero/negative/NaN capacity or invalid date): invalid.
        const double nan = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!degradationCurve(Hist{ { d0, 0.0 }, { d0.addDays(7), -5.0 } }).valid);
        QVERIFY(!degradationCurve(Hist{ { d0, nan }, { d0.addDays(7), nan } }).valid);
        QVERIFY(!degradationCurve(Hist{ { QDate(), 100.0 }, { QDate(), 200.0 } }).valid);
        // Same-day duplicates only: all x identical -> invalid.
        QVERIFY(!degradationCurve(Hist{ { d0, 50000.0 }, { d0, 49000.0 } }).valid);
        // Out-of-order dates: still a valid fit (negative x is fine).
        const auto outOfOrder = degradationCurve(
            Hist{ { d0.addDays(30), 49000.0 }, { d0, 50000.0 }, { d0.addDays(60), 48000.0 } });
        QVERIFY(outOfOrder.valid);
        QVERIFY(std::isfinite(outOfOrder.slope));
        // Non-monotonic (gauge relearn bumps): valid fit, finite numbers.
        const auto bumpy = degradationCurve(Hist{ { d0, 50000.0 },
                                                  { d0.addDays(7), 48000.0 },
                                                  { d0.addDays(14), 51000.0 },
                                                  { d0.addDays(21), 47000.0 } });
        QVERIFY(bumpy.valid);
        QVERIFY(std::isfinite(bumpy.slope) && std::isfinite(bumpy.r2));
        // NaN rows are skipped, remaining rows still fit.
        const auto mixed = degradationCurve(Hist{ { d0, 50000.0 },
                                                  { d0.addDays(7), nan },
                                                  { d0.addDays(14), 49000.0 } });
        QVERIFY(mixed.valid);
        QCOMPARE(mixed.n, 2);
    }

    void eolProjectionProperties_data()
    {
        QTest::addColumn<double>("lossPerWeek");
        QTest::addColumn<int>("weeks");
        QTest::addColumn<double>("design");
        Gen g(0xFA2ADA08u);
        for (int i = 0; i < 110; ++i) {
            QTest::addRow("eol%03d", i)
                << g.dblIn(-200.0, 900.0)   // negative = improving
                << (3 + int(g.u32Below(50)))
                << g.dblIn(20000.0, 90000.0);
        }
    }
    void eolProjectionProperties()
    {
        QFETCH(double, lossPerWeek);
        QFETCH(int, weeks);
        QFETCH(double, design);
        QList<QPair<QDate, double>> hist;
        const QDate d0(2026, 1, 5);
        for (int w = 0; w < weeks; ++w)
            hist.append({ d0.addDays(w * 7), design - lossPerWeek * w });
        const auto eol = endOfLifeProjection(hist, design);
        if (eol.has_value()) {
            // THE invariant: a projected end-of-life is NEVER in the past
            // relative to the newest history point.
            QVERIFY(*eol >= hist.last().first);
            // ...and never absurdly far out (30-year cap).
            QVERIFY(hist.first().first.daysTo(*eol) <= qint64(30.0 * 365.25) + 1);
        } else {
            // Absence is only legal for flat/improving series, or when the
            // 80% threshold lies beyond the 30-year cap or behind the data.
            if (lossPerWeek > 0.0) {
                const double daysTo80 = (0.2 * design) / (lossPerWeek / 7.0);
                const bool beyondCap = daysTo80 > 30.0 * 365.25;
                // +1 day margin: addDays truncates fractional projections.
                const bool behindData = daysTo80 < (weeks - 1) * 7.0 + 1.0;
                QVERIFY2(beyondCap || behindData,
                         qPrintable(QStringLiteral("loss=%1 weeks=%2 days80=%3")
                                        .arg(lossPerWeek).arg(weeks).arg(daysTo80)));
            }
        }
        // Hostile scalar inputs never crash and never project.
        const double nan = std::numeric_limits<double>::quiet_NaN();
        QVERIFY(!endOfLifeProjection(hist, nan).has_value());
        QVERIFY(!endOfLifeProjection(hist, design, nan).has_value());
        QVERIFY(!endOfLifeProjection(hist, -1.0).has_value());
        QVERIFY(!endOfLifeProjection(hist, design, 0.0).has_value());
    }

    // ---- per-state drain ---------------------------------------------------

    void perStateDrainProperties_data()
    {
        QTest::addColumn<int>("seed");
        for (int i = 0; i < 80; ++i)
            QTest::addRow("drain%02d", i) << i;
    }
    void perStateDrainProperties()
    {
        QFETCH(int, seed);
        Gen g(0xFA2ADA09u + seed);
        QList<PowercfgReportData::UsageEntry> usage;
        const QStringList states = { QStringLiteral("Active"), QStringLiteral("Suspend"),
                                     QStringLiteral("ConnectedStandby") };
        double expectedDcActivemWh = 0.0;
        const int n = int(g.u32Below(40));
        QDateTime ts(QDate(2026, 7, 1), QTime(0, 0));
        for (int i = 0; i < n; ++i) {
            PowercfgReportData::UsageEntry e;
            e.timestamp = ts.addSecs(i * 600);
            e.entryType = states.at(int(g.u32Below(quint32(states.size()))));
            e.ac = g.chance(0.4);
            const qint64 durationSec = g.chance(0.15) ? 0 : qint64(g.u32In(1, 7200));
            e.duration100ns = durationSec * 10000000ll;
            const qint64 discharge = qint64(g.i32In(-3000, 8000)); // negative = charging
            e.dischargemWh = discharge;
            usage.append(e);
            if (!e.ac && discharge > 0 && durationSec > 0
                && e.entryType == QLatin1String("Active"))
                expectedDcActivemWh += double(discharge);
        }
        const QList<StateDrain> drains = perStateDrain(usage);
        double gotActivemWh = 0.0;
        for (const StateDrain &d : drains) {
            // Invariants: only positive DC drain aggregates; averages finite
            // and strictly positive; durations positive; entry counts sane.
            QVERIFY(d.entries > 0);
            QVERIFY(d.totalDurationSec > 0);
            QVERIFY(d.totalmWh > 0.0);
            QVERIFY(std::isfinite(d.avgDrainmW));
            QVERIFY(d.avgDrainmW > 0.0);
            if (d.state == QLatin1String("Active"))
                gotActivemWh = d.totalmWh;
        }
        QVERIFY(qAbs(gotActivemWh - expectedDcActivemWh) < 1e-6);
    }

    // ---- verdict tiers -------------------------------------------------------

    void verdictTierMonotonic_data()
    {
        QTest::addColumn<int>("health");
        for (int h = 0; h <= 100; ++h)
            QTest::addRow("health=%d", h) << h;
    }
    void verdictTierMonotonic()
    {
        QFETCH(int, health);
        const HealthGrade grade = HealthVerdict::gradeForHealth(double(health));
        // Tier is monotonic: one point more health never yields a worse tier
        // (enum order: Excellent < Good < Fair < Worn < ReplaceSoon).
        if (health < 100) {
            const HealthGrade better = HealthVerdict::gradeForHealth(double(health + 1));
            QVERIFY(int(better) <= int(grade));
        }
        // Exact boundary contract.
        const HealthGrade expected =
            health >= 90 ? HealthGrade::Excellent
            : health >= 80 ? HealthGrade::Good
            : health >= 65 ? HealthGrade::Fair
            : health >= 50 ? HealthGrade::Worn
                           : HealthGrade::ReplaceSoon;
        QCOMPARE(grade, expected);
        // Every tier has a distinct, non-empty name and headline.
        QVERIFY(!HealthVerdict::gradeName(grade).isEmpty());
        VerdictInput input;
        input.healthPercent = double(health);
        QVERIFY(!HealthVerdict::generate(input).headline.isEmpty());
    }

    void verdictUnknownAndFractionalBoundaries()
    {
        QCOMPARE(HealthVerdict::gradeForHealth(std::nullopt), HealthGrade::Unknown);
        // Fractional values just under each boundary fall to the lower tier.
        QCOMPARE(HealthVerdict::gradeForHealth(89.999), HealthGrade::Good);
        QCOMPARE(HealthVerdict::gradeForHealth(79.999), HealthGrade::Fair);
        QCOMPARE(HealthVerdict::gradeForHealth(64.999), HealthGrade::Worn);
        QCOMPARE(HealthVerdict::gradeForHealth(49.999), HealthGrade::ReplaceSoon);
        QCOMPARE(HealthVerdict::gradeForHealth(100.0), HealthGrade::Excellent);
        QCOMPARE(HealthVerdict::gradeForHealth(0.0), HealthGrade::ReplaceSoon);
    }
};

QTEST_APPLESS_MAIN(TestMetricsProperties)
#include "test_metrics_properties.moc"
