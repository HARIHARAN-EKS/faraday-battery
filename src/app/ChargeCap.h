#pragma once

#include <QString>
#include <optional>

namespace faraday {

// Firmware charge-cap (e.g. "stop charging at 80%") detection.
//
// Charge caps are vendor firmware features with no standard Windows API.
// The only widely documented user-space surface is Lenovo's BIOS WMI
// interface (Lenovo_BiosSetting / Lenovo_SetBiosSetting in ROOT\WMI).
// probe() detects it read-only; setEnabled() attempts the documented
// Lenovo write path and reports failure cleanly (it usually requires
// elevation, which Faraday never requests). On every other platform the
// feature reports unsupported and the UI hides it entirely.
struct ChargeCapInfo
{
    bool supported = false;
    QString vendor;                 // "Lenovo" when detected
    QString settingName;            // BIOS setting that carries the cap
    std::optional<bool> enabled;    // current state when readable
    QString detail;                 // human-readable status / hint
};

class ChargeCap
{
public:
    static ChargeCapInfo probe();
    static bool setEnabled(bool enable, QString *errorOut = nullptr);
};

} // namespace faraday
