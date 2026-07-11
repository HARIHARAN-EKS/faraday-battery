#pragma once

#include "acquisition/PowercfgReport.h"

#include <QDateTime>
#include <QList>
#include <QPointF>
#include <QString>
#include <QVector>
#include <optional>

namespace faraday {
namespace metrics {

// --- Basic health -----------------------------------------------------------

// 100 * fullCharged / design, clamped to [0, 100]. nullopt on bad input.
std::optional<double> healthPercent(std::optional<quint32> fullChargedmWh,
                                    std::optional<quint32> designmWh);

// 100 - healthPercent.
std::optional<double> wearPercent(std::optional<quint32> fullChargedmWh,
                                  std::optional<quint32> designmWh);

// --- Live power --------------------------------------------------------------

// Signed net power in watts: positive while charging, negative while
// discharging, 0 when idle/full. Rates come from BatteryStatus in mW.
std::optional<double> netPowerW(std::optional<qint32> chargeRatemW,
                                std::optional<qint32> dischargeRatemW);

// P = V * I for sources that expose current instead of power.
std::optional<double> powerFromVoltageCurrentW(std::optional<quint32> voltagemV,
                                               std::optional<qint32> currentmA);

// Charge percentage computed from coulometry (remaining / fullCharged).
std::optional<double> computedChargePercent(std::optional<quint32> remainingmWh,
                                            std::optional<quint32> fullChargedmWh);

// Difference between the OS-reported percentage and the computed one.
// Large values (> ~5) indicate the fuel gauge needs calibration.
std::optional<double> calibrationDriftPercent(std::optional<int> reportedPct,
                                              std::optional<quint32> remainingmWh,
                                              std::optional<quint32> fullChargedmWh);

// --- Time estimates ----------------------------------------------------------

// Instantaneous estimates from the current draw. nullopt when the rate is
// zero/absent or inputs are inconsistent.
std::optional<qint64> timeToFullSec(std::optional<quint32> remainingmWh,
                                    std::optional<quint32> fullChargedmWh,
                                    std::optional<qint32> chargeRatemW);
std::optional<qint64> timeToEmptySec(std::optional<quint32> remainingmWh,
                                     std::optional<qint32> dischargeRatemW);

// --- Regression / projections -------------------------------------------------

struct RegressionResult
{
    double slope = 0.0;      // y units per x unit
    double intercept = 0.0;
    double r2 = 0.0;
    int n = 0;
    bool valid = false;
};

// Ordinary least squares over (x, y) points. Needs >= 2 distinct x values.
RegressionResult linearRegression(const QVector<QPointF> &points);

// Extrapolated time-to-empty: regression over recent (elapsedSec,
// remainingmWh) samples. Returns seconds from the last sample until the
// fitted line reaches zero; nullopt if not discharging.
std::optional<qint64> extrapolatedTimeToEmptySec(const QVector<QPointF> &samples);

// Degradation: regression of (daysSinceFirstPoint, fullChargemWh) capacity
// history. slope is mWh/day (negative when degrading).
RegressionResult degradationCurve(const QList<QPair<QDate, double>> &history);

// Projected date when capacity crosses thresholdPercent of design.
// nullopt when history is flat/improving or insufficient.
std::optional<QDate> endOfLifeProjection(const QList<QPair<QDate, double>> &history,
                                         double designmWh,
                                         double thresholdPercent = 80.0);

// --- Per-state drain ----------------------------------------------------------

struct StateDrain
{
    QString state;            // Active / Suspend / ...
    double avgDrainmW = 0.0;  // mean drain while on battery in this state
    double totalmWh = 0.0;    // total energy drawn on battery in this state
    qint64 totalDurationSec = 0;
    int entries = 0;
};

// Aggregates powercfg usage entries (DC only, positive discharge) by state.
QList<StateDrain> perStateDrain(const QList<PowercfgReportData::UsageEntry> &usage);

} // namespace metrics
} // namespace faraday
