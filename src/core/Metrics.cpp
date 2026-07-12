#include "Metrics.h"

#include <QHash>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace faraday {
namespace metrics {

namespace {

const qint64 kSecondsPerHour = 3600;
const qint64 kMaxEstimateSec = 60ll * 60ll * 24ll * 7ll; // cap estimates at a week

} // namespace

std::optional<double> healthPercent(std::optional<quint32> fullChargedmWh,
                                    std::optional<quint32> designmWh)
{
    if (!fullChargedmWh.has_value() || !designmWh.has_value() || *designmWh == 0)
        return std::nullopt;
    const double pct = 100.0 * static_cast<double>(*fullChargedmWh)
                             / static_cast<double>(*designmWh);
    return std::clamp(pct, 0.0, 100.0);
}

std::optional<double> wearPercent(std::optional<quint32> fullChargedmWh,
                                  std::optional<quint32> designmWh)
{
    const auto health = healthPercent(fullChargedmWh, designmWh);
    if (!health.has_value())
        return std::nullopt;
    return 100.0 - *health;
}

std::optional<double> netPowerW(std::optional<qint32> chargeRatemW,
                                std::optional<qint32> dischargeRatemW)
{
    if (!chargeRatemW.has_value() && !dischargeRatemW.has_value())
        return std::nullopt;
    const double charge = chargeRatemW.value_or(0);
    const double discharge = dischargeRatemW.value_or(0);
    return (charge - discharge) / 1000.0;
}

std::optional<double> powerFromVoltageCurrentW(std::optional<quint32> voltagemV,
                                               std::optional<qint32> currentmA)
{
    if (!voltagemV.has_value() || !currentmA.has_value())
        return std::nullopt;
    // (mV * mA) / 1e6 = W
    return static_cast<double>(*voltagemV) * static_cast<double>(*currentmA) / 1e6;
}

std::optional<double> computedChargePercent(std::optional<quint32> remainingmWh,
                                            std::optional<quint32> fullChargedmWh)
{
    if (!remainingmWh.has_value() || !fullChargedmWh.has_value() || *fullChargedmWh == 0)
        return std::nullopt;
    const double pct = 100.0 * static_cast<double>(*remainingmWh)
                             / static_cast<double>(*fullChargedmWh);
    return std::clamp(pct, 0.0, 100.0);
}

std::optional<double> calibrationDriftPercent(std::optional<int> reportedPct,
                                              std::optional<quint32> remainingmWh,
                                              std::optional<quint32> fullChargedmWh)
{
    const auto computed = computedChargePercent(remainingmWh, fullChargedmWh);
    if (!reportedPct.has_value() || !computed.has_value())
        return std::nullopt;
    if (*reportedPct < 0 || *reportedPct > 100)
        return std::nullopt;
    return static_cast<double>(*reportedPct) - *computed;
}

std::optional<qint64> timeToFullSec(std::optional<quint32> remainingmWh,
                                    std::optional<quint32> fullChargedmWh,
                                    std::optional<qint32> chargeRatemW)
{
    if (!remainingmWh.has_value() || !fullChargedmWh.has_value() || !chargeRatemW.has_value())
        return std::nullopt;
    if (*chargeRatemW <= 0 || *fullChargedmWh <= *remainingmWh)
        return std::nullopt;
    const double deficitmWh = static_cast<double>(*fullChargedmWh - *remainingmWh);
    const qint64 sec = static_cast<qint64>(deficitmWh / *chargeRatemW * kSecondsPerHour);
    if (sec < 0 || sec > kMaxEstimateSec)
        return std::nullopt;
    return sec;
}

std::optional<qint64> timeToEmptySec(std::optional<quint32> remainingmWh,
                                     std::optional<qint32> dischargeRatemW)
{
    if (!remainingmWh.has_value() || !dischargeRatemW.has_value())
        return std::nullopt;
    if (*dischargeRatemW <= 0)
        return std::nullopt;
    const qint64 sec = static_cast<qint64>(
        static_cast<double>(*remainingmWh) / *dischargeRatemW * kSecondsPerHour);
    if (sec < 0 || sec > kMaxEstimateSec)
        return std::nullopt;
    return sec;
}

RegressionResult linearRegression(const QVector<QPointF> &points)
{
    RegressionResult result;
    result.n = points.size();
    if (points.size() < 2)
        return result;

    double sumX = 0, sumY = 0;
    for (const QPointF &p : points) {
        // A single NaN/inf sample would otherwise poison every sum and
        // still exit through the "valid" path (NaN comparisons are false).
        if (!std::isfinite(p.x()) || !std::isfinite(p.y()))
            return result;
        sumX += p.x();
        sumY += p.y();
    }
    const double meanX = sumX / points.size();
    const double meanY = sumY / points.size();

    double sxx = 0, sxy = 0, syy = 0;
    for (const QPointF &p : points) {
        const double dx = p.x() - meanX;
        const double dy = p.y() - meanY;
        sxx += dx * dx;
        sxy += dx * dy;
        syy += dy * dy;
    }
    if (sxx <= 0.0)
        return result; // all x identical: vertical line, no fit

    result.slope = sxy / sxx;
    result.intercept = meanY - result.slope * meanX;
    // r2 = 1 for a perfectly flat series (syy == 0): the fit is exact.
    result.r2 = (syy > 0.0) ? (sxy * sxy) / (sxx * syy) : 1.0;
    result.valid = true;
    return result;
}

std::optional<qint64> extrapolatedTimeToEmptySec(const QVector<QPointF> &samples)
{
    const RegressionResult fit = linearRegression(samples);
    if (!fit.valid || fit.slope >= 0.0)
        return std::nullopt; // not discharging (or charging)

    const QPointF &last = samples.last();
    const double fittedNow = fit.slope * last.x() + fit.intercept;
    if (fittedNow <= 0.0)
        return 0;
    const double secondsLeft = -fittedNow / fit.slope;
    if (secondsLeft < 0 || secondsLeft > static_cast<double>(kMaxEstimateSec))
        return std::nullopt;
    return static_cast<qint64>(secondsLeft);
}

RegressionResult degradationCurve(const QList<QPair<QDate, double>> &history)
{
    QVector<QPointF> points;
    points.reserve(history.size());
    if (history.isEmpty())
        return RegressionResult();

    const QDate origin = history.first().first;
    for (const auto &entry : history) {
        // NaN compares false against <= 0.0, so the finiteness check must
        // be explicit or a NaN capacity would reach the regression.
        if (!entry.first.isValid() || !std::isfinite(entry.second) || entry.second <= 0.0)
            continue;
        points.append(QPointF(static_cast<double>(origin.daysTo(entry.first)), entry.second));
    }
    return linearRegression(points);
}

std::optional<QDate> endOfLifeProjection(const QList<QPair<QDate, double>> &history,
                                         double designmWh,
                                         double thresholdPercent)
{
    // Non-finite inputs would flow through to a static_cast<qint64>(NaN)
    // in addDays (undefined behavior); reject them up front.
    if (!std::isfinite(designmWh) || !std::isfinite(thresholdPercent))
        return std::nullopt;
    if (designmWh <= 0.0 || thresholdPercent <= 0.0 || history.isEmpty())
        return std::nullopt;

    const RegressionResult fit = degradationCurve(history);
    if (!fit.valid || fit.slope >= 0.0)
        return std::nullopt; // flat or improving: no meaningful projection

    const double targetmWh = designmWh * thresholdPercent / 100.0;
    const double daysFromOrigin = (targetmWh - fit.intercept) / fit.slope;
    const QDate origin = history.first().first;

    // Reject projections in the past or beyond 30 years (fit noise).
    if (daysFromOrigin < 0 || daysFromOrigin > 30.0 * 365.25)
        return std::nullopt;
    const QDate projected = origin.addDays(static_cast<qint64>(daysFromOrigin));
    if (projected < history.last().first)
        return std::nullopt;
    return projected;
}

QList<StateDrain> perStateDrain(const QList<PowercfgReportData::UsageEntry> &usage)
{
    struct Acc
    {
        double totalmWh = 0.0;
        qint64 totalDurationSec = 0;
        int entries = 0;
    };
    QHash<QString, Acc> byState;
    QStringList order;

    for (const auto &entry : usage) {
        if (entry.ac)
            continue; // drain is only meaningful on battery
        if (!entry.dischargemWh.has_value() || *entry.dischargemWh <= 0)
            continue; // negative = charging, zero = no draw
        const qint64 durationSec = entry.duration100ns / 10000000ll;
        if (durationSec <= 0)
            continue;
        if (!byState.contains(entry.entryType))
            order.append(entry.entryType);
        Acc &acc = byState[entry.entryType];
        acc.totalmWh += static_cast<double>(*entry.dischargemWh);
        acc.totalDurationSec += durationSec;
        acc.entries += 1;
    }

    QList<StateDrain> result;
    for (const QString &state : order) {
        const Acc &acc = byState.value(state);
        StateDrain drain;
        drain.state = state;
        drain.totalmWh = acc.totalmWh;
        drain.totalDurationSec = acc.totalDurationSec;
        drain.entries = acc.entries;
        drain.avgDrainmW = acc.totalmWh / (static_cast<double>(acc.totalDurationSec) / 3600.0);
        result.append(drain);
    }
    return result;
}

} // namespace metrics
} // namespace faraday
