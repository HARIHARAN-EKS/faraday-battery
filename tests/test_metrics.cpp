#include <QtTest/QtTest>

#include "core/Metrics.h"

using namespace faraday;
using namespace faraday::metrics;

namespace {

PowercfgReportData::UsageEntry usageEntry(bool ac, const QString &type,
                                          qint64 dischargemWh, qint64 durationSec,
                                          qint64 chargemWh = 20000, qint64 fccmWh = 40000)
{
    PowercfgReportData::UsageEntry e;
    e.timestamp = QDateTime(QDate(2026, 7, 1), QTime(12, 0));
    e.entryType = type;
    e.ac = ac;
    e.dischargemWh = dischargemWh;
    e.duration100ns = durationSec * 10000000ll;
    e.chargeCapacitymWh = chargemWh;
    e.fullChargeCapacitymWh = fccmWh;
    return e;
}

} // namespace

class TestMetrics : public QObject
{
    Q_OBJECT
private slots:
    void healthPercentFormula()
    {
        QCOMPARE(healthPercent(quint32(42401), quint32(42401)).value_or(0), 100.0);
        QVERIFY(qAbs(healthPercent(quint32(40000), quint32(42401)).value_or(0) - 94.337) < 0.01);
        QCOMPARE(healthPercent(quint32(45000), quint32(42401)).value_or(0), 100.0); // clamped
        QVERIFY(!healthPercent(std::nullopt, quint32(42401)).has_value());
        QVERIFY(!healthPercent(quint32(40000), std::nullopt).has_value());
        QVERIFY(!healthPercent(quint32(40000), quint32(0)).has_value()); // div by zero
    }

    void wearPercentFormula()
    {
        QVERIFY(qAbs(wearPercent(quint32(40000), quint32(42401)).value_or(0) - 5.663) < 0.01);
        QCOMPARE(wearPercent(quint32(42401), quint32(42401)).value_or(-1), 0.0);
        QVERIFY(!wearPercent(std::nullopt, quint32(1)).has_value());
    }

    void netPowerFormula()
    {
        QCOMPARE(netPowerW(qint32(31664), qint32(0)).value_or(0), 31.664);   // charging
        QCOMPARE(netPowerW(qint32(0), qint32(8500)).value_or(0), -8.5);      // discharging
        QCOMPARE(netPowerW(qint32(0), qint32(0)).value_or(-1), 0.0);         // idle
        QVERIFY(!netPowerW(std::nullopt, std::nullopt).has_value());
        QCOMPARE(netPowerW(qint32(5000), std::nullopt).value_or(0), 5.0);    // partial data
    }

    void voltageCurrentPower()
    {
        QCOMPARE(powerFromVoltageCurrentW(quint32(12686), qint32(2000)).value_or(0), 25.372);
        QVERIFY(!powerFromVoltageCurrentW(std::nullopt, qint32(2000)).has_value());
        QVERIFY(!powerFromVoltageCurrentW(quint32(12686), std::nullopt).has_value());
    }

    void computedChargeFormula()
    {
        QVERIFY(qAbs(computedChargePercent(quint32(32288), quint32(42401)).value_or(0) - 76.149) < 0.01);
        QCOMPARE(computedChargePercent(quint32(50000), quint32(42401)).value_or(0), 100.0);
        QVERIFY(!computedChargePercent(quint32(1), quint32(0)).has_value());
    }

    void calibrationDriftFormula()
    {
        // Reported 76 %, computed 76.149 % -> drift ~ -0.149
        const auto drift = calibrationDriftPercent(76, quint32(32288), quint32(42401));
        QVERIFY(drift.has_value());
        QVERIFY(qAbs(*drift + 0.149) < 0.01);
        QVERIFY(!calibrationDriftPercent(101, quint32(1), quint32(2)).has_value());
        QVERIFY(!calibrationDriftPercent(-1, quint32(1), quint32(2)).has_value());
        QVERIFY(!calibrationDriftPercent(std::nullopt, quint32(1), quint32(2)).has_value());
    }

    void timeToFullFormula()
    {
        // (42401 - 32288) mWh at 31664 mW -> 1149.6 s
        const auto ttf = timeToFullSec(quint32(32288), quint32(42401), qint32(31664));
        QVERIFY(ttf.has_value());
        QVERIFY(qAbs(*ttf - 1149) <= 1);
        QVERIFY(!timeToFullSec(quint32(42401), quint32(42401), qint32(31664)).has_value()); // full
        QVERIFY(!timeToFullSec(quint32(32288), quint32(42401), qint32(0)).has_value());     // no rate
        QVERIFY(!timeToFullSec(quint32(32288), quint32(42401), qint32(-5)).has_value());
        // Absurdly slow charge (> 1 week estimate) is rejected
        QVERIFY(!timeToFullSec(quint32(0), quint32(42401000), qint32(1)).has_value());
    }

    void timeToEmptyFormula()
    {
        // 32288 mWh at 8000 mW -> 14529.6 s
        const auto tte = timeToEmptySec(quint32(32288), qint32(8000));
        QVERIFY(tte.has_value());
        QVERIFY(qAbs(*tte - 14529) <= 1);
        QVERIFY(!timeToEmptySec(quint32(32288), qint32(0)).has_value());
        QVERIFY(!timeToEmptySec(std::nullopt, qint32(8000)).has_value());
        QVERIFY(!timeToEmptySec(quint32(42401000), qint32(1)).has_value()); // > 1 week
    }

    void regressionPerfectLine()
    {
        QVector<QPointF> pts{ {0, 1}, {1, 3}, {2, 5}, {3, 7} }; // y = 2x + 1
        const RegressionResult r = linearRegression(pts);
        QVERIFY(r.valid);
        QCOMPARE(r.n, 4);
        QVERIFY(qAbs(r.slope - 2.0) < 1e-9);
        QVERIFY(qAbs(r.intercept - 1.0) < 1e-9);
        QVERIFY(qAbs(r.r2 - 1.0) < 1e-9);
    }

    void regressionDegenerateInputs()
    {
        QVERIFY(!linearRegression({}).valid);
        QVERIFY(!linearRegression({ QPointF(1, 1) }).valid);
        QVERIFY(!linearRegression({ QPointF(1, 1), QPointF(1, 5) }).valid); // same x
        const RegressionResult flat = linearRegression({ QPointF(0, 4), QPointF(10, 4) });
        QVERIFY(flat.valid);
        QCOMPARE(flat.slope, 0.0);
        QCOMPARE(flat.r2, 1.0);
    }

    void extrapolatedTimeToEmpty()
    {
        // 40000 mWh at t=0 draining 10 mWh/s -> empty 4000 s after t=0,
        // i.e. 3700 s after the last sample at t=300.
        QVector<QPointF> samples{ {0, 40000}, {100, 39000}, {200, 38000}, {300, 37000} };
        const auto tte = extrapolatedTimeToEmptySec(samples);
        QVERIFY(tte.has_value());
        QVERIFY(qAbs(*tte - 3700) <= 1);

        // Charging series has no time-to-empty.
        QVector<QPointF> charging{ {0, 30000}, {100, 31000} };
        QVERIFY(!extrapolatedTimeToEmptySec(charging).has_value());

        // Already at/below zero extrapolates to 0.
        QVector<QPointF> dead{ {0, 100}, {100, 0} };
        QCOMPARE(extrapolatedTimeToEmptySec(dead).value_or(-1), qint64(0));
    }

    void degradationCurveFromHistory()
    {
        QList<QPair<QDate, double>> history{
            { QDate(2026, 1, 1), 42000.0 },
            { QDate(2026, 2, 1), 41700.0 },
            { QDate(2026, 3, 1), 41400.0 },
        };
        const RegressionResult r = degradationCurve(history);
        QVERIFY(r.valid);
        QVERIFY(r.slope < 0.0);
        QVERIFY(qAbs(r.slope + 10.135) < 0.1); // ~ -600 mWh over 59.2 days avg
    }

    void endOfLifeProjectionFormula()
    {
        // 42000 at day 0, declining exactly 10 mWh/day. Design 42401,
        // threshold 80 % -> target 33920.8 -> day 807.9 from origin.
        QList<QPair<QDate, double>> history{
            { QDate(2026, 1, 1), 42000.0 },
            { QDate(2026, 4, 11), 41000.0 }, // exactly 100 days later
        };
        const auto eol = endOfLifeProjection(history, 42401.0, 80.0);
        QVERIFY(eol.has_value());
        QCOMPARE(*eol, QDate(2026, 1, 1).addDays(807));

        // Improving capacity: no projection.
        QList<QPair<QDate, double>> improving{
            { QDate(2026, 1, 1), 41000.0 },
            { QDate(2026, 2, 1), 42000.0 },
        };
        QVERIFY(!endOfLifeProjection(improving, 42401.0, 80.0).has_value());

        QVERIFY(!endOfLifeProjection(history, 0.0, 80.0).has_value());
        QVERIFY(!endOfLifeProjection({}, 42401.0, 80.0).has_value());
    }

    void perStateDrainAggregation()
    {
        QList<PowercfgReportData::UsageEntry> usage;
        usage << usageEntry(false, QStringLiteral("Active"), 1000, 3600)   // 1000 mW
              << usageEntry(false, QStringLiteral("Active"), 500, 1800)    // 1000 mW
              << usageEntry(false, QStringLiteral("Suspend"), 50, 3600)    // 50 mW
              << usageEntry(true, QStringLiteral("Active"), 2000, 3600)    // AC: skipped
              << usageEntry(false, QStringLiteral("Active"), -300, 3600);  // charging: skipped

        const QList<StateDrain> drains = perStateDrain(usage);
        QCOMPARE(drains.size(), 2);
        QCOMPARE(drains.at(0).state, QStringLiteral("Active"));
        QCOMPARE(drains.at(0).entries, 2);
        QCOMPARE(drains.at(0).totalmWh, 1500.0);
        QCOMPARE(drains.at(0).totalDurationSec, qint64(5400));
        QVERIFY(qAbs(drains.at(0).avgDrainmW - 1000.0) < 1e-9);
        QCOMPARE(drains.at(1).state, QStringLiteral("Suspend"));
        QVERIFY(qAbs(drains.at(1).avgDrainmW - 50.0) < 1e-9);

        QVERIFY(perStateDrain({}).isEmpty());
    }
};

QTEST_APPLESS_MAIN(TestMetrics)
#include "test_metrics.moc"
