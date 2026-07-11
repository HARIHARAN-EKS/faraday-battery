#include "BatteryReader.h"

namespace faraday {

namespace {

const quint32 kRuntimeUnknown = 0xFFFFFFFFu;
const quint32 kRuntimeMaxPlausibleSec = 60u * 60u * 24u * 7u; // one week

std::optional<quint32> optUInt(const QVariantMap &row, const QString &key)
{
    const auto it = row.constFind(key);
    if (it == row.constEnd() || !it->isValid())
        return std::nullopt;
    bool convOk = false;
    const quint32 v = it->toUInt(&convOk);
    if (!convOk)
        return std::nullopt;
    return v;
}

std::optional<qint32> optInt(const QVariantMap &row, const QString &key)
{
    const auto it = row.constFind(key);
    if (it == row.constEnd() || !it->isValid())
        return std::nullopt;
    bool convOk = false;
    const qint32 v = it->toInt(&convOk);
    if (!convOk)
        return std::nullopt;
    return v;
}

std::optional<bool> optBool(const QVariantMap &row, const QString &key)
{
    const auto it = row.constFind(key);
    if (it == row.constEnd() || !it->isValid())
        return std::nullopt;
    return it->toBool();
}

QString optString(const QVariantMap &row, const QString &key)
{
    const auto it = row.constFind(key);
    if (it == row.constEnd() || !it->isValid())
        return QString();
    return it->toString().trimmed();
}

} // namespace

BatteryReader::BatteryReader()
    : m_rootWmi(new WmiClient()),
      m_cimv2(new WmiClient())
{
}

BatteryReader::~BatteryReader() = default;

bool BatteryReader::initialize()
{
    m_rootWmiOk = m_rootWmi->connect(QStringLiteral("ROOT\\WMI"));
    m_cimv2Ok = m_cimv2->connect(QStringLiteral("ROOT\\CIMV2"));
    return m_rootWmiOk || m_cimv2Ok;
}

double BatteryReader::thermalRawToCelsius(quint32 raw)
{
    return raw / 10.0 - 273.15;
}

bool BatteryReader::thermalRawIsValid(quint32 raw)
{
    // Raw <= 2732 (i.e. <= 273.2 K ~ 0 C) reads as a firmware stub zone;
    // > 120 C is not a sane temperature for a battery monitor to report.
    // Compare on the raw integer to avoid floating-point edge cases.
    return raw > 2732 && thermalRawToCelsius(raw) <= 120.0;
}

std::optional<quint32> BatteryReader::sanitizeRuntime(quint32 seconds)
{
    if (seconds == kRuntimeUnknown || seconds == 0 || seconds > kRuntimeMaxPlausibleSec)
        return std::nullopt;
    return seconds;
}

QString BatteryReader::chemistryToString(int code)
{
    switch (code) {
    case 1: return QStringLiteral("Other");
    case 2: return QStringLiteral("Unknown");
    case 3: return QStringLiteral("Lead Acid");
    case 4: return QStringLiteral("Nickel Cadmium");
    case 5: return QStringLiteral("Nickel Metal Hydride");
    case 6: return QStringLiteral("Lithium-ion");
    case 7: return QStringLiteral("Zinc Air");
    case 8: return QStringLiteral("Lithium Polymer");
    default: return QString();
    }
}

QString BatteryReader::win32StatusToString(int code)
{
    switch (code) {
    case 1: return QStringLiteral("Discharging");
    case 2: return QStringLiteral("On AC power");
    case 3: return QStringLiteral("Fully charged");
    case 4: return QStringLiteral("Low");
    case 5: return QStringLiteral("Critical");
    case 6: return QStringLiteral("Charging");
    case 7: return QStringLiteral("Charging (high)");
    case 8: return QStringLiteral("Charging (low)");
    case 9: return QStringLiteral("Charging (critical)");
    case 10: return QStringLiteral("Undefined");
    case 11: return QStringLiteral("Partially charged");
    default: return QString();
    }
}

bool BatteryReader::snapshotHasRealBattery(const BatterySnapshot &snapshot)
{
    for (const BatteryDevice &b : snapshot.batteries) {
        if (b.fullChargedCapacitymWh.has_value() && *b.fullChargedCapacitymWh > 0)
            return true;
        if (b.designedCapacitymWh.has_value() && *b.designedCapacitymWh > 0)
            return true;
        // Win32_Battery-only systems: a live percentage still counts.
        if (b.estimatedChargeRemainingPct.has_value())
            return true;
    }
    return false;
}

BatteryDevice &BatteryReader::deviceFor(BatterySnapshot &snapshot, const QString &instanceName)
{
    for (BatteryDevice &b : snapshot.batteries) {
        if (b.instanceName == instanceName)
            return b;
    }
    BatteryDevice fresh;
    fresh.instanceName = instanceName;
    snapshot.batteries.append(fresh);
    return snapshot.batteries.last();
}

void BatteryReader::mergeRootWmi(BatterySnapshot &snapshot)
{
    if (!m_rootWmiOk) {
        snapshot.unavailable << QStringLiteral("ROOT\\WMI: %1").arg(m_rootWmi->lastError());
        return;
    }

    struct ClassSpec {
        const char *className;
        const char *label;
    };
    // BatteryStaticData frequently fails without elevation ("Generic
    // failure"); each class is fetched and recorded independently.
    const ClassSpec classes[] = {
        { "BatteryStaticData", "BatteryStaticData" },
        { "BatteryFullChargedCapacity", "BatteryFullChargedCapacity" },
        { "BatteryCycleCount", "BatteryCycleCount" },
        { "BatteryStatus", "BatteryStatus" },
        { "BatteryRuntime", "BatteryRuntime" },
    };

    for (const ClassSpec &spec : classes) {
        bool ok = false;
        const QList<QVariantMap> rows =
            m_rootWmi->query(QStringLiteral("SELECT * FROM %1").arg(QLatin1String(spec.className)), &ok);
        if (!ok) {
            snapshot.unavailable << QStringLiteral("%1: %2")
                                        .arg(QLatin1String(spec.label), m_rootWmi->lastError());
            continue;
        }
        for (const QVariantMap &row : rows) {
            const QString instance = optString(row, QStringLiteral("InstanceName"));
            if (instance.isEmpty())
                continue;
            BatteryDevice &dev = deviceFor(snapshot, instance);

            if (qstrcmp(spec.className, "BatteryStaticData") == 0) {
                dev.designedCapacitymWh = optUInt(row, QStringLiteral("DesignedCapacity"));
                const QString mfg = optString(row, QStringLiteral("ManufactureName"));
                if (!mfg.isEmpty())
                    dev.manufacturer = mfg;
                const QString serial = optString(row, QStringLiteral("SerialNumber"));
                if (!serial.isEmpty())
                    dev.serialNumber = serial;
                const QString devName = optString(row, QStringLiteral("DeviceName"));
                if (!devName.isEmpty())
                    dev.name = devName;
            } else if (qstrcmp(spec.className, "BatteryFullChargedCapacity") == 0) {
                dev.fullChargedCapacitymWh = optUInt(row, QStringLiteral("FullChargedCapacity"));
            } else if (qstrcmp(spec.className, "BatteryCycleCount") == 0) {
                dev.cycleCount = optUInt(row, QStringLiteral("CycleCount"));
            } else if (qstrcmp(spec.className, "BatteryStatus") == 0) {
                dev.remainingCapacitymWh = optUInt(row, QStringLiteral("RemainingCapacity"));
                dev.chargeRatemW = optInt(row, QStringLiteral("ChargeRate"));
                dev.dischargeRatemW = optInt(row, QStringLiteral("DischargeRate"));
                dev.voltagemV = optUInt(row, QStringLiteral("Voltage"));
                dev.powerOnline = optBool(row, QStringLiteral("PowerOnline"));
                dev.charging = optBool(row, QStringLiteral("Charging"));
                dev.discharging = optBool(row, QStringLiteral("Discharging"));
                dev.critical = optBool(row, QStringLiteral("Critical"));
            } else if (qstrcmp(spec.className, "BatteryRuntime") == 0) {
                const std::optional<quint32> raw = optUInt(row, QStringLiteral("EstimatedRuntime"));
                dev.estimatedRuntimeSec = raw.has_value() ? sanitizeRuntime(*raw) : std::nullopt;
            }
        }
    }
}

void BatteryReader::mergeCimv2(BatterySnapshot &snapshot)
{
    if (!m_cimv2Ok) {
        snapshot.unavailable << QStringLiteral("ROOT\\CIMV2: %1").arg(m_cimv2->lastError());
        return;
    }

    bool ok = false;
    const QList<QVariantMap> win32 =
        m_cimv2->query(QStringLiteral("SELECT * FROM Win32_Battery"), &ok);
    if (!ok)
        snapshot.unavailable << QStringLiteral("Win32_Battery: %1").arg(m_cimv2->lastError());

    bool okPortable = false;
    const QList<QVariantMap> portable =
        m_cimv2->query(QStringLiteral("SELECT * FROM Win32_PortableBattery"), &okPortable);
    if (!okPortable)
        snapshot.unavailable << QStringLiteral("Win32_PortableBattery: %1").arg(m_cimv2->lastError());

    // ROOT\WMI and CIMV2 use unrelated instance keys, so align by ordinal;
    // in practice enumeration order matches on multi-battery systems.
    const int count = qMax(win32.size(), portable.size());
    for (int i = 0; i < count; ++i) {
        BatteryDevice *dev = nullptr;
        if (i < snapshot.batteries.size()) {
            dev = &snapshot.batteries[i];
        } else {
            QString key;
            if (i < win32.size())
                key = optString(win32.at(i), QStringLiteral("DeviceID"));
            if (key.isEmpty() && i < portable.size())
                key = optString(portable.at(i), QStringLiteral("DeviceID"));
            if (key.isEmpty())
                key = QStringLiteral("Battery %1").arg(i);
            dev = &deviceFor(snapshot, key);
        }

        if (i < win32.size()) {
            const QVariantMap &row = win32.at(i);
            if (dev->name.isEmpty())
                dev->name = optString(row, QStringLiteral("Name"));
            dev->win32Status = optInt(row, QStringLiteral("BatteryStatus"));
            dev->estimatedChargeRemainingPct =
                optInt(row, QStringLiteral("EstimatedChargeRemaining"));
            if (!dev->designVoltagemV.has_value())
                dev->designVoltagemV = optUInt(row, QStringLiteral("DesignVoltage"));
            if (dev->chemistry.isEmpty()) {
                const std::optional<qint32> chem = optInt(row, QStringLiteral("Chemistry"));
                if (chem.has_value())
                    dev->chemistry = chemistryToString(*chem);
            }
        }

        if (i < portable.size()) {
            const QVariantMap &row = portable.at(i);
            if (dev->manufacturer.isEmpty())
                dev->manufacturer = optString(row, QStringLiteral("Manufacturer"));
            if (dev->name.isEmpty())
                dev->name = optString(row, QStringLiteral("Name"));
            if (!dev->designedCapacitymWh.has_value())
                dev->designedCapacitymWh = optUInt(row, QStringLiteral("DesignCapacity"));
            if (!dev->fullChargedCapacitymWh.has_value())
                dev->fullChargedCapacitymWh = optUInt(row, QStringLiteral("FullChargeCapacity"));
            if (dev->chemistry.isEmpty()) {
                const std::optional<qint32> chem = optInt(row, QStringLiteral("Chemistry"));
                if (chem.has_value())
                    dev->chemistry = chemistryToString(*chem);
            }
        }
    }
}

void BatteryReader::mergeThermal(BatterySnapshot &snapshot)
{
    if (!m_rootWmiOk)
        return; // already reported by mergeRootWmi

    bool ok = false;
    const QList<QVariantMap> rows = m_rootWmi->query(
        QStringLiteral("SELECT * FROM MsAcpi_ThermalZoneTemperature"), &ok);
    if (!ok) {
        snapshot.unavailable << QStringLiteral("MsAcpi_ThermalZoneTemperature: %1")
                                    .arg(m_rootWmi->lastError());
        return;
    }

    double validSum = 0.0;
    int validCount = 0;
    std::optional<double> batteryZoneTemp;

    for (const QVariantMap &row : rows) {
        ThermalZone zone;
        zone.instanceName = optString(row, QStringLiteral("InstanceName"));
        const std::optional<quint32> raw = optUInt(row, QStringLiteral("CurrentTemperature"));
        if (raw.has_value()) {
            zone.celsius = thermalRawToCelsius(*raw);
            zone.valid = thermalRawIsValid(*raw);
        }
        zone.isBatteryZone = zone.instanceName.contains(QStringLiteral("BAT"), Qt::CaseInsensitive);
        if (zone.valid) {
            validSum += zone.celsius;
            ++validCount;
            if (zone.isBatteryZone && !batteryZoneTemp.has_value())
                batteryZoneTemp = zone.celsius;
        }
        snapshot.thermalZones.append(zone);
    }

    if (batteryZoneTemp.has_value()) {
        snapshot.temperatureC = batteryZoneTemp;
    } else if (validCount > 0) {
        // No battery-specific zone exposed: report the mean of the valid
        // ACPI zones as a system thermal estimate.
        snapshot.temperatureC = validSum / validCount;
    } else if (!rows.isEmpty()) {
        snapshot.unavailable << QStringLiteral("MsAcpi_ThermalZoneTemperature: only stub zones present");
    } else {
        snapshot.unavailable << QStringLiteral("MsAcpi_ThermalZoneTemperature: no instances");
    }
}

BatterySnapshot BatteryReader::read()
{
    BatterySnapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTime();

    mergeRootWmi(snapshot);
    mergeCimv2(snapshot);
    mergeThermal(snapshot);

    snapshot.batteryPresent = snapshotHasRealBattery(snapshot);
    return snapshot;
}

} // namespace faraday
