#pragma once

#include "BatteryTypes.h"
#include "WmiClient.h"

#include <memory>

namespace faraday {

// Merges every available battery data source into a BatterySnapshot.
//
// Sources (all read-only, each individually optional):
//   ROOT\WMI    : BatteryStaticData, BatteryFullChargedCapacity,
//                 BatteryCycleCount, BatteryStatus, BatteryRuntime,
//                 MsAcpi_ThermalZoneTemperature
//   ROOT\CIMV2  : Win32_Battery, Win32_PortableBattery
//
// Thread affinity: initialize() and read() must run on the same thread
// (COM apartment requirement). Never throws.
class BatteryReader
{
public:
    BatteryReader();
    ~BatteryReader();

    bool initialize();
    BatterySnapshot read();

    // Pure helpers, exposed for unit testing.

    // MsAcpi_ThermalZoneTemperature.CurrentTemperature is tenths of Kelvin.
    static double thermalRawToCelsius(quint32 raw);

    // Zones stuck at exactly 273.2 K (raw 2732) or reporting absurd values
    // are firmware stubs and must be discarded.
    static bool thermalRawIsValid(quint32 raw);

    // BatteryRuntime.EstimatedRuntime is 0xFFFFFFFF when unknown (on AC).
    static std::optional<quint32> sanitizeRuntime(quint32 seconds);

    // Win32_PortableBattery.Chemistry enum (1..8) to a display string.
    static QString chemistryToString(int code);

    // Win32_Battery.BatteryStatus enum (1..11) to a display string.
    static QString win32StatusToString(int code);

    // A pack counts as present only with a positive full-charge or design
    // capacity from any source (VMs expose ghost instances with zeros).
    static bool snapshotHasRealBattery(const BatterySnapshot &snapshot);

private:
    void mergeRootWmi(BatterySnapshot &snapshot);
    void mergeCimv2(BatterySnapshot &snapshot);
    void mergeThermal(BatterySnapshot &snapshot);
    BatteryDevice &deviceFor(BatterySnapshot &snapshot, const QString &instanceName);

    std::unique_ptr<WmiClient> m_rootWmi;
    std::unique_ptr<WmiClient> m_cimv2;
    bool m_rootWmiOk = false;
    bool m_cimv2Ok = false;
};

} // namespace faraday
