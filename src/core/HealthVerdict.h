#pragma once

#include "Metrics.h"

#include <QString>
#include <QStringList>
#include <optional>

namespace faraday {

enum class HealthGrade
{
    Unknown,
    Excellent,  // >= 90 % of design capacity
    Good,       // >= 80 %
    Fair,       // >= 65 %
    Worn,       // >= 50 %
    ReplaceSoon // < 50 %
};

struct VerdictInput
{
    std::optional<double> healthPercent;
    std::optional<quint32> cycleCount;
    std::optional<double> temperatureC;
    std::optional<double> calibrationDriftPercent;
    metrics::RegressionResult degradation;    // slope in mWh/day
    std::optional<QDate> endOfLife;
};

struct Verdict
{
    HealthGrade grade = HealthGrade::Unknown;
    QString headline;       // one-sentence plain-language summary
    QStringList details;    // supporting plain-language observations
};

// Rule-based, deterministic plain-language health verdict.
// All strings pass through Qt's translation system.
class HealthVerdict
{
public:
    static HealthGrade gradeForHealth(std::optional<double> healthPercent);
    static QString gradeName(HealthGrade grade);
    static Verdict generate(const VerdictInput &input);

    // Charging-habit insights derived from the powercfg usage log.
    // Rules: long dwell at >= 95 % on AC, deep discharges below 15 %,
    // and a summary of time on battery vs plugged in.
    static QStringList chargingHabitInsights(
        const QList<PowercfgReportData::UsageEntry> &usage);
};

} // namespace faraday
