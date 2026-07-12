# Data sources

Faraday reads hardware data exclusively through public, user-mode, read-only
Windows interfaces. No drivers, no kernel access, no elevation.

## 1. WMI — ROOT\WMI namespace (ACPI battery miniclass)

Queried with `SELECT * FROM <class>` through `IWbemServices::ExecQuery`
(forward-only enumerator). Instances are keyed by `InstanceName`
(e.g. `ACPI\PNP0C0A\1_0`); multiple batteries appear as multiple instances.

| Class | Fields used | Notes / observed failure modes |
|---|---|---|
| `BatteryStaticData` | `DesignedCapacity`, `ManufactureName`, `SerialNumber`, `DeviceName`, `UniqueID`, `ManufactureDate` (CIM datetime; all-wildcard when unreported), `Chemistry` (FourCC, e.g. `0x6E6F694C` = "Lion") | **Intermittently fails with `WBEM_E_FAILED` 0x80041001 — see below; this is NOT an elevation requirement.** Faraday retries via `CreateInstanceEnum` and caches the last good rows (static per boot); powercfg backfills when nothing was ever readable |
| `BatteryFullChargedCapacity` | `FullChargedCapacity` | The primary health input |
| `BatteryCycleCount` | `CycleCount` | Zero/absent on some firmware |
| `BatteryStatus` | `RemainingCapacity`, `ChargeRate`, `DischargeRate`, `Voltage`, `PowerOnline`, `Charging`, `Discharging`, `Critical` | The live-telemetry workhorse; rates in mW, voltage in mV |
| `BatteryRuntime` | `EstimatedRuntime` (s) | `0xFFFFFFFF` = unknown (typical on AC) — treated as absent |
| `MsAcpi_ThermalZoneTemperature` | `CurrentTemperature` (tenths of Kelvin), `InstanceName` | Many machines expose stub zones frozen at 2732 (0.0 °C) — filtered out. Battery-specific zones (`*BAT*`) are preferred; otherwise the mean of valid zones is a *system* estimate |

### The 0x80041001 finding (verification round)

`BatteryStaticData` inherits from `Win32_PerfRawData` and is served through
the WMI **performance adapter**, which intermittently returns
`WBEM_E_FAILED` (0x80041001). During the original build this was observed
consistently on the reference machine and initially attributed to missing
elevation. The verification round disproved that:

- `Get-WmiObject` (legacy DCOM/`IWbemServices` path) read the class
  **unelevated** — full data including `DesignedCapacity` 42581,
  `ManufactureName` "Hewlett-Packard", serial and `UniqueID`.
- `Get-CimInstance` (WinRM/CIM path) failed with 0x80041001 on the same
  machine at the same moment.
- A minimal C++ probe then showed **every** DCOM enumeration style working
  (`ExecQuery` forward-only/rewindable/synchronous and `CreateInstanceEnum`),
  confirming the earlier in-app failures were transient adapter hiccups, not
  a path or privilege problem.

Resolution (all read-only, still `asInvoker`):

1. `WmiClient::queryInstances()` retries a failed `ExecQuery` through
   `IWbemServices::CreateInstanceEnum` with a rewindable enumerator.
2. `BatteryReader` caches the last successful `BatteryStaticData` rows —
   the data is static per boot — so a transient failure on sample N reuses
   the cache instead of blanking pack identity, and a failure on the first
   read simply retries on the next sample.
3. Fields the pack genuinely does not report (here: `ManufactureDate`, an
   all-asterisk CIM datetime) render as "Not reported by this hardware" —
   never blank, zero, or guessed.

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
