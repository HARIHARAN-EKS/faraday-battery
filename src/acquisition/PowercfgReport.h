#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <optional>

namespace faraday {

// Data extracted from `powercfg /batteryreport /xml`.
struct PowercfgReportData
{
    bool ok = false;
    QString error;

    struct BatteryInfo
    {
        QString id;
        QString manufacturer;
        QString serialNumber;
        QString chemistry;
        std::optional<qint64> designCapacitymWh;
        std::optional<qint64> fullChargeCapacitymWh;
        std::optional<qint64> cycleCount;
    };
    QList<BatteryInfo> batteries;

    // Weekly (or daily, near the present) capacity-history buckets.
    struct HistoryEntry
    {
        QDateTime start;
        QDateTime end;
        std::optional<qint64> designCapacitymWh;
        std::optional<qint64> fullChargeCapacitymWh;
        std::optional<qint64> cycleCount;
    };
    QList<HistoryEntry> history;

    // Recent usage log (last ~3 days of state transitions).
    struct UsageEntry
    {
        QDateTime timestamp;      // local time
        QString entryType;        // Active / Suspend / ...
        bool ac = false;
        std::optional<qint64> chargeCapacitymWh;
        std::optional<qint64> dischargemWh;   // negative while charging
        std::optional<qint64> fullChargeCapacitymWh;
        qint64 duration100ns = 0;
    };
    QList<UsageEntry> usage;
};

// Runs powercfg (a stock read-only Windows tool) and parses its XML output.
// generate() writes the temporary XML inside workDir and removes it after
// parsing. parse() is pure and fully unit-testable. Neither throws.
class PowercfgReport
{
public:
    static PowercfgReportData generate(const QString &workDir, int timeoutMs = 30000);
    static PowercfgReportData parse(const QByteArray &xml);
};

} // namespace faraday
