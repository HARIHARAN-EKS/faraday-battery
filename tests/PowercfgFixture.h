#pragma once

// Sample battery-report XML matching the real powercfg schema.
// Kept in a separate translation unit: moc cannot parse multi-line raw
// string literals and would otherwise skip the test class entirely.
extern const char kSampleBatteryReportXml[];
