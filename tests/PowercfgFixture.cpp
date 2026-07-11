#include "PowercfgFixture.h"

// Trimmed fixture matching the real powercfg /batteryreport /xml schema
// captured on Windows 11 (element-based <Battery>, attribute-based entries).
const char kSampleBatteryReportXml[] = R"(<?xml version="1.0" encoding="utf-8"?>
<BatteryReport xmlns="http://schemas.microsoft.com/battery/2012">
  <SystemInformation>
    <ComputerName>TESTBOX</ComputerName>
  </SystemInformation>
  <Batteries>
    <Battery>
      <Id>Primary</Id>
      <Manufacturer>Hewlett-Packard</Manufacturer>
      <SerialNumber>32872 08/21/2025</SerialNumber>
      <ManufactureDate></ManufactureDate>
      <Chemistry>LIon</Chemistry>
      <LongTerm>1</LongTerm>
      <RelativeCapacity>0</RelativeCapacity>
      <DesignCapacity>42401</DesignCapacity>
      <FullChargeCapacity>40000</FullChargeCapacity>
      <CycleCount>35</CycleCount>
    </Battery>
  </Batteries>
  <RecentUsage>
    <UsageEntry
      Timestamp="2026-07-11T15:01:52Z"
      LocalTimestamp="2026-07-11T20:31:52"
      Duration="298972668"
      Ac="1"
      EntryType="Active"
      ChargeCapacity="22455"
      Discharge="-236"
      FullChargeCapacity="42401"
      IsNextOnBattery="0"
      />
    <UsageEntry
      Timestamp="2026-07-11T15:02:22Z"
      LocalTimestamp="2026-07-11T20:32:22"
      Duration="1260494806"
      Ac="0"
      EntryType="Suspend"
      ChargeCapacity="22691"
      Discharge="990"
      FullChargeCapacity="42401"
      IsNextOnBattery="0"
      />
  </RecentUsage>
  <History>
    <HistoryEntry
      StartDate="2026-06-01T11:56:48Z"
      LocalStartDate="2026-06-01T17:26:48"
      EndDate="2026-06-08T11:56:48Z"
      LocalEndDate="2026-06-08T17:26:48"
      DesignCapacity="42413"
      FullChargeCapacity="42413"
      CycleCount="34"
      BatteryChanged="0"
      />
    <HistoryEntry
      StartDate="2026-06-08T11:56:48Z"
      LocalStartDate="2026-06-08T17:26:48"
      EndDate="2026-06-15T11:56:48Z"
      LocalEndDate="2026-06-15T17:26:48"
      DesignCapacity="42413"
      FullChargeCapacity="42000"
      CycleCount="34"
      BatteryChanged="0"
      />
  </History>
</BatteryReport>
)";
