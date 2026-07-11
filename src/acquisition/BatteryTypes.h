#pragma once

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <optional>

namespace faraday {

// One physical battery pack, merged from ROOT\WMI and ROOT\CIMV2 sources.
// Every field that comes from hardware is optional: absence means the
// source class/property was missing, failed, or returned a sentinel.
struct BatteryDevice
{
    QString instanceName;   // ACPI instance (ROOT\WMI key) or Win32 DeviceID fallback

    // Static / metadata
    QString name;
    QString manufacturer;
    QString serialNumber;
    QString chemistry;
    std::optional<quint32> designedCapacitymWh;    // BatteryStaticData.DesignedCapacity
    std::optional<quint32> fullChargedCapacitymWh; // BatteryFullChargedCapacity
    std::optional<quint32> cycleCount;             // BatteryCycleCount
    std::optional<quint32> designVoltagemV;        // Win32_Battery.DesignVoltage

    // Live status
    std::optional<quint32> remainingCapacitymWh;   // BatteryStatus.RemainingCapacity
    std::optional<qint32>  chargeRatemW;           // BatteryStatus.ChargeRate
    std::optional<qint32>  dischargeRatemW;        // BatteryStatus.DischargeRate
    std::optional<quint32> voltagemV;              // BatteryStatus.Voltage
    std::optional<bool>    powerOnline;
    std::optional<bool>    charging;
    std::optional<bool>    discharging;
    std::optional<bool>    critical;
    std::optional<quint32> estimatedRuntimeSec;    // BatteryRuntime (sentinel-filtered)
    std::optional<int>     win32Status;            // Win32_Battery.BatteryStatus 1..11
    std::optional<int>     estimatedChargeRemainingPct;
};

struct ThermalZone
{
    QString instanceName;
    double celsius = 0.0;
    bool valid = false;     // false for stub zones (raw <= 273.2 K) or absurd values
    bool isBatteryZone = false;
};

struct BatterySnapshot
{
    QDateTime timestamp;
    bool batteryPresent = false;
    QList<BatteryDevice> batteries;
    QList<ThermalZone> thermalZones;
    std::optional<double> temperatureC;  // battery zone if present, else system estimate
    QStringList unavailable;             // human-readable list of failed sources
};

} // namespace faraday

Q_DECLARE_METATYPE(faraday::BatterySnapshot)
