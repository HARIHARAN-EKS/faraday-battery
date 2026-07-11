# Data sources

Faraday reads hardware data exclusively through public, user-mode, read-only
Windows interfaces. No drivers, no kernel access, no elevation.

## 1. WMI — ROOT\WMI namespace (ACPI battery miniclass)

Queried with `SELECT * FROM <class>` through `IWbemServices::ExecQuery`
(forward-only enumerator). Instances are keyed by `InstanceName`
(e.g. `ACPI\PNP0C0A\1_0`); multiple batteries appear as multiple instances.

| Class | Fields used | Notes / observed failure modes |
|---|---|---|
| `BatteryStaticData` | `DesignedCapacity`, `ManufactureName`, `SerialNumber`, `DeviceName` | **Frequently fails without elevation** (`WBEM_E_FAILED` 0x80041001, observed on the reference HP machine). Faraday falls back to powercfg / `Win32_PortableBattery` for design capacity |
| `BatteryFullChargedCapacity` | `FullChargedCapacity` | The primary health input |
| `BatteryCycleCount` | `CycleCount` | Zero/absent on some firmware |
| `BatteryStatus` | `RemainingCapacity`, `ChargeRate`, `DischargeRate`, `Voltage`, `PowerOnline`, `Charging`, `Discharging`, `Critical` | The live-telemetry workhorse; rates in mW, voltage in mV |
| `BatteryRuntime` | `EstimatedRuntime` (s) | `0xFFFFFFFF` = unknown (typical on AC) — treated as absent |
| `MsAcpi_ThermalZoneTemperature` | `CurrentTemperature` (tenths of Kelvin), `InstanceName` | Many machines expose stub zones frozen at 2732 (0.0 °C) — filtered out. Battery-specific zones (`*BAT*`) are preferred; otherwise the mean of valid zones is a *system* estimate |

## 2. WMI — ROOT\CIMV2 namespace

| Class | Fields used | Notes |
|---|---|---|
| `Win32_Battery` | `Name`, `BatteryStatus` (enum 1–11), `EstimatedChargeRemaining` (%), `DesignVoltage` (mV), `Chemistry` | The status enum mapping ships in `BatteryReader::win32StatusToString` (1 Discharging … 11 Partially charged) |
| `Win32_PortableBattery` | `Manufacturer`, `Name`, `DesignCapacity`, `FullChargeCapacity`, `Chemistry` (enum 1–8) | Fallback metadata; on the reference machine `FullChargeCapacity` was empty and `DesignCapacity` (41040) disagreed slightly with powercfg (42401) — powercfg wins |

`ROOT\WMI` and `ROOT\CIMV2` use unrelated instance keys, so packs are aligned
by ordinal, which matches enumeration order in practice.

## 3. powercfg battery report

`%WINDIR%\System32\powercfg.exe /batteryreport /xml /output <temp file>` —
a stock, unprivileged, read-only Windows tool. The XML (namespace
`http://schemas.microsoft.com/battery/2012`) provides:

- `<Batteries><Battery>` (child elements): id, manufacturer, serial, chemistry,
  **DesignCapacity**, **FullChargeCapacity**, **CycleCount**.
- `<History><HistoryEntry>` (attributes): weekly buckets with
  `StartDate/EndDate` (+ `Local*` variants), `DesignCapacity`,
  `FullChargeCapacity`, `CycleCount` → Faraday's capacity-history store.
- `<RecentUsage><UsageEntry>` (attributes): `Timestamp/LocalTimestamp`,
  `EntryType` (Active/Suspend/…), `Ac` (0/1), `ChargeCapacity`,
  `Discharge` (mWh, negative while charging), `FullChargeCapacity`,
  `Duration` (100 ns ticks) → usage log, per-state drain, habit insights.

The temporary XML is parsed with `QXmlStreamReader` and deleted immediately.
Desktops still produce a valid report with an empty `<Batteries>` element.

## 4. Vendor firmware (charge cap) — detection only

`Lenovo_BiosSetting` (ROOT\WMI, documented Lenovo BIOS WMI interface) is probed
read-only for settings named like *Battery Conservation Mode* /
*ChargeThreshold*. Only when such a setting exists does the UI surface the
charge-cap section; changing it requires the vendor path (usually elevated),
which Faraday reports honestly instead of attempting silently. On all other
hardware the section is hidden.

## 5. What Faraday deliberately does NOT use

- No NT kernel IOCTLs (`IOCTL_BATTERY_*`) — WMI covers the same data without a
  device handle.
- No undocumented ACPI methods, no embedded-controller pokes, no SMBus access.
- No network sources of any kind.

## No-battery / VM detection

A system is treated as battery-less when no source yields a positive
full-charge or design capacity and no live percentage exists (VM ghost
batteries report zeros). The UI then shows a clean "No battery detected"
state and live monitoring is disabled.
