#pragma once

// Test-only synthetic battery data injector. Builds fabricated snapshots,
// powercfg reports and usage logs so every degradation path can be exercised
// on healthy reference hardware. Lives in tests/ and links only into test
// binaries — it is never compiled into the shipped application.

#include "acquisition/BatteryTypes.h"
#include "acquisition/PowercfgReport.h"

#include <QDate>
#include <QDateTime>
#include <QList>

namespace faraday {
namespace synthetic {

// One battery pack at a given wear level. remaining is derived from the
// full-charge capacity so the computed charge percentage equals
// chargePercent unless reportedPct deliberately disagrees (drift tests).
inline BatteryDevice pack(double wearPercent,
                          double chargePercent = 60.0,
                          bool onAc = false,
                          quint32 designmWh = 50000,
                          int reportedPct = -1,
                          int index = 0)
{
    BatteryDevice dev;
    dev.instanceName = QStringLiteral("SYNTH\\BAT\\%1_0").arg(index);
    dev.name = QStringLiteral("Synthetic %1").arg(index);
    dev.designedCapacitymWh = designmWh;
    const quint32 fcc =
        static_cast<quint32>(qRound64(designmWh * (1.0 - wearPercent / 100.0)));
    dev.fullChargedCapacitymWh = fcc;
    dev.remainingCapacitymWh =
        static_cast<quint32>(qRound64(fcc * chargePercent / 100.0));
    dev.powerOnline = onAc;
    dev.charging = false;
    dev.discharging = !onAc;
    dev.chargeRatemW = 0;
    dev.dischargeRatemW = onAc ? 0 : 8000;
    dev.voltagemV = 11400;
    dev.cycleCount = 120;
    dev.estimatedChargeRemainingPct =
        reportedPct >= 0 ? reportedPct : static_cast<int>(chargePercent);
    return dev;
}

// A full snapshot with one pack per wear value.
inline BatterySnapshot snapshot(const QList<double> &wearPercents,
                                double chargePercent = 60.0,
                                bool onAc = false,
                                quint32 designmWh = 50000)
{
    BatterySnapshot snap;
    snap.timestamp = QDateTime::currentDateTime();
    snap.batteryPresent = true;
    for (int i = 0; i < wearPercents.size(); ++i)
        snap.batteries.append(
            pack(wearPercents.at(i), chargePercent, onAc, designmWh, -1, i));
    return snap;
}

// A VM-style ghost battery: instance exists, every capacity is zero.
inline BatterySnapshot ghostSnapshot()
{
    BatterySnapshot snap;
    snap.timestamp = QDateTime::currentDateTime();
    BatteryDevice ghost;
    ghost.instanceName = QStringLiteral("ACPI\\PNP0C0A\\0");
    ghost.designedCapacitymWh = 0;
    ghost.fullChargedCapacitymWh = 0;
    snap.batteries.append(ghost);
    snap.batteryPresent = false; // what BatteryReader::snapshotHasRealBattery yields
    return snap;
}

// A powercfg report whose weekly capacity history declines linearly.
inline PowercfgReportData decliningReport(int weeks,
                                          qint64 designmWh,
                                          qint64 startFccmWh,
                                          qint64 lossPerWeekmWh,
                                          const QDate &startDate = QDate(2026, 1, 5))
{
    PowercfgReportData report;
    report.ok = true;

    PowercfgReportData::BatteryInfo info;
    info.id = QStringLiteral("SYNTH-1");
    info.manufacturer = QStringLiteral("Synthetic Cells Inc.");
    info.chemistry = QStringLiteral("LIon");
    info.designCapacitymWh = designmWh;
    info.fullChargeCapacitymWh = startFccmWh - lossPerWeekmWh * (weeks - 1);
    info.cycleCount = 120;
    report.batteries.append(info);

    for (int i = 0; i < weeks; ++i) {
        PowercfgReportData::HistoryEntry h;
        h.start = QDateTime(startDate.addDays(i * 7), QTime(0, 0));
        h.end = h.start.addDays(7);
        h.designCapacitymWh = designmWh;
        h.fullChargeCapacitymWh = startFccmWh - lossPerWeekmWh * i;
        h.cycleCount = 100 + i;
        report.history.append(h);
    }
    return report;
}

// One usage-log entry (duration in seconds; dischargemWh positive = drain).
inline PowercfgReportData::UsageEntry usageEntry(const QDateTime &ts,
                                                 const QString &type,
                                                 bool ac,
                                                 qint64 durationSec,
                                                 qint64 dischargemWh,
                                                 qint64 chargeCapacitymWh = 30000,
                                                 qint64 fccmWh = 50000)
{
    PowercfgReportData::UsageEntry e;
    e.timestamp = ts;
    e.entryType = type;
    e.ac = ac;
    e.duration100ns = durationSec * 10000000ll;
    e.dischargemWh = dischargemWh;
    e.chargeCapacitymWh = chargeCapacitymWh;
    e.fullChargeCapacitymWh = fccmWh;
    return e;
}

} // namespace synthetic
} // namespace faraday
