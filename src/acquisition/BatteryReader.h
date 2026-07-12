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
// BatteryStaticData is served through the WMI performance adapter, which
// intermittently fails with WBEM_E_FAILED (0x80041001). Because the data is
// static, successful rows are cached: a later transient failure reuses the
// cache instead of blanking the pack metadata, and a failure on the first
// read is retried on every subsequent sample.
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

    // BatteryStaticData.Chemistry is a FourCC (ACPI _BIF battery type
    // string packed little-endian into a UInt32, e.g. 0x6E6F694C = "Lion").
    // Returns a friendly name for known codes, the raw ASCII for unknown
    // printable codes, and an empty string for garbage.
    static QString chemistryFourCCToString(quint32 code);

    // Normalizes short chemistry tokens from powercfg/ACPI ("LIon", "LiP",
    // "PbAc", ...) to the same friendly names; unknown tokens pass through.
    static QString normalizeChemistryToken(const QString &token);

    // BatteryStaticData.ManufactureDate is a CIM datetime string
    // ("yyyymmddHHMMSS.mmmmmm+UUU", unknown fields as asterisks).
    // Returns ISO "yyyy-MM-dd" when a plausible date is present, else empty.
    static QString manufactureDateToIso(const QString &cimDateTime);

    // Win32_Battery.BatteryStatus enum (1..11) to a display string.
    static QString win32StatusToString(int code);

    // A pack counts as present only with a positive full-charge or design
    // capacity from any source (VMs expose ghost instances with zeros).
    static bool snapshotHasRealBattery(const BatterySnapshot &snapshot);

    // Resolves snapshot.temperatureC / temperatureIsEstimate from the
    // already-populated snapshot.thermalZones: a valid *BAT* zone is a true
    // battery sensor; otherwise the mean of the valid zones is a SYSTEM
    // estimate (temperatureIsEstimate = true). Appends to
    // snapshot.unavailable when only stub zones (or none) exist.
    static void resolveTemperature(BatterySnapshot &snapshot);

private:
    void mergeRootWmi(BatterySnapshot &snapshot);
    void mergeCimv2(BatterySnapshot &snapshot);
    void mergeThermal(BatterySnapshot &snapshot);
    void applyStaticRows(BatterySnapshot &snapshot, const QList<QVariantMap> &rows);
    BatteryDevice &deviceFor(BatterySnapshot &snapshot, const QString &instanceName);

    std::unique_ptr<WmiClient> m_rootWmi;
    std::unique_ptr<WmiClient> m_cimv2;
    bool m_rootWmiOk = false;
    bool m_cimv2Ok = false;

    // Last successful BatteryStaticData rows (static per boot, safe to reuse).
    QList<QVariantMap> m_staticCache;
};

} // namespace faraday
