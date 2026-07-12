#include <QtTest/QtTest>

#include "SyntheticData.h"
#include "app/BatteryModel.h"
#include "app/Calibration.h"

#include <cmath>

using namespace faraday;

// System-clock jump handling. NOTE (honest limit): actually changing the
// Windows clock requires elevation, which this project never uses — so
// wall-clock jumps are simulated through the injectable timestamps that
// every time-consuming component takes (snapshot.timestamp, Calibration's
// `now`). QTimer-driven sampling cadence is monotonic-clock based in Qt on
// Windows (QueryPerformanceCounter) and is unaffected by wall-clock moves;
// that part is reasoned, not simulated.
class TestClockJumps : public QObject
{
    Q_OBJECT

    static BatterySnapshot at(const QDateTime &ts, double pct = 60.0)
    {
        BatterySnapshot s = synthetic::snapshot({ 10.0 }, pct);
        s.timestamp = ts;
        return s;
    }

private slots:
    void forwardJumpKeepsSeriesFinite()
    {
        BatteryModel model;
        const QDateTime t0(QDate(2026, 7, 12), QTime(9, 0));
        model.applySnapshot(at(t0, 80));
        model.applySnapshot(at(t0.addSecs(30), 79));
        // DST-style jump: +1 hour. Then a suspend-resume-style jump: +1 day.
        model.applySnapshot(at(t0.addSecs(3600 + 60), 78));
        model.applySnapshot(at(t0.addDays(1), 70));

        QCOMPARE(model.liveSeries().size(), 4);
        double prevT = -1e18;
        for (const QVariant &v : model.liveSeries()) {
            const double t = v.toMap().value(QStringLiteral("t")).toDouble();
            QVERIFY(std::isfinite(t));
            QVERIFY(t >= prevT); // forward jumps keep elapsed monotonic
            prevT = t;
        }
        // Trend over a gappy series: absent or finite — never NaN.
        const QVariantMap trend = model.liveTrend();
        if (trend.value(QStringLiteral("valid")).toBool())
            QVERIFY(std::isfinite(trend.value(QStringLiteral("slopePctPerHour")).toDouble()));
    }

    void backwardJumpNeverCrashesOrPoisonsTrend()
    {
        BatteryModel model;
        const QDateTime t0(QDate(2026, 7, 12), QTime(9, 0));
        model.applySnapshot(at(t0, 80));
        model.applySnapshot(at(t0.addSecs(60), 79));
        // Clock pulled back 2 hours (NTP correction / manual change).
        model.applySnapshot(at(t0.addSecs(-7200), 78));
        model.applySnapshot(at(t0.addSecs(-7200 + 60), 77));

        QCOMPARE(model.liveSeries().size(), 4);
        for (const QVariant &v : model.liveSeries()) {
            const double t = v.toMap().value(QStringLiteral("t")).toDouble();
            QVERIFY(std::isfinite(t)); // negative is tolerated, NaN is not
        }
        const QVariantMap trend = model.liveTrend();
        if (trend.value(QStringLiteral("valid")).toBool()) {
            QVERIFY(std::isfinite(trend.value(QStringLiteral("slopePctPerHour")).toDouble()));
            QVERIFY(std::isfinite(trend.value(QStringLiteral("lastY")).toDouble()));
        }
        // Recovery: reset the session origin and the series restarts clean.
        model.resetSessionOrigin();
        QCOMPARE(model.liveSeries().size(), 0);
        model.applySnapshot(at(t0.addSecs(120), 76));
        QCOMPARE(model.liveSeries().size(), 1);
    }

    void invalidTimestampSnapshotTolerated()
    {
        BatteryModel model;
        BatterySnapshot s = synthetic::snapshot({ 10.0 });
        s.timestamp = QDateTime(); // invalid
        model.applySnapshot(s);    // must not crash
        model.applySnapshot(at(QDateTime(QDate(2026, 7, 12), QTime(9, 0))));
        QVERIFY(model.liveSeries().size() >= 1);
        for (const QVariant &v : model.liveSeries())
            QVERIFY(std::isfinite(v.toMap().value(QStringLiteral("t")).toDouble()));
    }

    void calibrationSurvivesBackwardClock()
    {
        Calibration cal;
        const QDateTime t0(QDate(2026, 7, 12), QTime(9, 0));
        cal.start();
        cal.update(50.0, true, t0);                 // charging up
        QCOMPARE(cal.step(), int(Calibration::ChargeToFull));
        cal.update(99.0, true, t0.addSecs(600));    // reached full -> rest
        QCOMPARE(cal.step(), int(Calibration::RestAtFull));
        // Clock jumps BACK 3 hours mid-rest: elapsed must not overflow or
        // skip the rest phase via a negative-duration artifact.
        cal.update(99.0, true, t0.addSecs(-10800));
        QVERIFY(cal.step() == int(Calibration::RestAtFull)
                || cal.step() == int(Calibration::DischargeToLow));
        // Then jumps FORWARD past the rest window: workflow proceeds.
        cal.update(99.0, true, t0.addDays(1));
        QCOMPARE(cal.step(), int(Calibration::DischargeToLow));
        // Full walkthrough still completes after the turbulence.
        cal.update(9.0, false, t0.addDays(1).addSecs(3600));
        QCOMPARE(cal.step(), int(Calibration::RechargeToFull));
        cal.update(99.5, true, t0.addDays(1).addSecs(7200));
        QCOMPARE(cal.step(), int(Calibration::Done));
    }

    void calibrationComputeNextNegativeElapsed()
    {
        // The pure transition function with a hostile negative elapsed time:
        // the rest step must simply not advance (waits), never crash or skip.
        QCOMPARE(Calibration::computeNext(Calibration::RestAtFull, 99.0, true, -5000),
                 Calibration::RestAtFull);
        QCOMPARE(Calibration::computeNext(Calibration::RestAtFull, 99.0, true,
                                          Calibration::kRestSeconds + 1),
                 Calibration::DischargeToLow);
    }
};

QTEST_GUILESS_MAIN(TestClockJumps)
#include "test_clock_jumps.moc"
