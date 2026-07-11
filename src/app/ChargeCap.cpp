#include "ChargeCap.h"

#include "acquisition/WmiClient.h"

#include <QCoreApplication>

namespace faraday {

namespace {

// Lenovo BIOS settings that represent a charge cap / conservation mode.
const char *kLenovoSettingCandidates[] = {
    "Battery Conservation Mode",
    "BatteryConservationMode",
    "ChargeThreshold",
    "Battery Charge Threshold",
};

} // namespace

ChargeCapInfo ChargeCap::probe()
{
    ChargeCapInfo info;

    WmiClient wmi;
    if (!wmi.connect(QStringLiteral("ROOT\\WMI")))
        return info;

    bool ok = false;
    const QList<QVariantMap> rows =
        wmi.query(QStringLiteral("SELECT CurrentSetting FROM Lenovo_BiosSetting"), &ok);
    if (!ok || rows.isEmpty())
        return info; // class absent on non-Lenovo firmware -> unsupported

    for (const QVariantMap &row : rows) {
        // Format: "SettingName,Value" (documented Lenovo BIOS WMI format).
        const QString setting = row.value(QStringLiteral("CurrentSetting")).toString();
        const int comma = setting.indexOf(QLatin1Char(','));
        if (comma <= 0)
            continue;
        const QString name = setting.left(comma).trimmed();
        const QString value = setting.mid(comma + 1).trimmed();
        for (const char *candidate : kLenovoSettingCandidates) {
            if (name.compare(QLatin1String(candidate), Qt::CaseInsensitive) == 0) {
                info.supported = true;
                info.vendor = QStringLiteral("Lenovo");
                info.settingName = name;
                if (value.compare(QLatin1String("Enable"), Qt::CaseInsensitive) == 0
                    || value.compare(QLatin1String("Enabled"), Qt::CaseInsensitive) == 0) {
                    info.enabled = true;
                } else if (value.compare(QLatin1String("Disable"), Qt::CaseInsensitive) == 0
                           || value.compare(QLatin1String("Disabled"), Qt::CaseInsensitive) == 0) {
                    info.enabled = false;
                }
                info.detail = QCoreApplication::translate(
                    "ChargeCap", "Firmware setting \"%1\" is %2.").arg(name, value);
                return info;
            }
        }
    }
    return info;
}

bool ChargeCap::setEnabled(bool enable, QString *errorOut)
{
    const ChargeCapInfo info = probe();
    if (!info.supported) {
        if (errorOut)
            *errorOut = QCoreApplication::translate(
                "ChargeCap", "This platform does not expose a charge-cap control.");
        return false;
    }

    // Documented Lenovo write path. Without elevation the provider refuses,
    // and Faraday deliberately never elevates - report the outcome honestly.
    Q_UNUSED(enable);
    if (errorOut)
        *errorOut = QCoreApplication::translate(
            "ChargeCap",
            "Changing \"%1\" requires the vendor tool or an elevated session; "
            "Faraday runs without elevation by design. Current state: %2")
            .arg(info.settingName, info.detail);
    return false;
}

} // namespace faraday
