#include "PowercfgReport.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QXmlStreamReader>

namespace faraday {

namespace {

std::optional<qint64> toOptInt64(const QString &text)
{
    if (text.isEmpty())
        return std::nullopt;
    bool convOk = false;
    const qint64 v = text.toLongLong(&convOk);
    if (!convOk)
        return std::nullopt;
    return v;
}

QDateTime parseTimestamp(const QString &text)
{
    // Report carries both "2026-07-11T20:31:52" (local) and
    // "2026-07-11T15:01:52Z" (UTC) formats.
    QDateTime dt = QDateTime::fromString(text, Qt::ISODate);
    return dt;
}

QString powercfgPath()
{
    QString windir = qEnvironmentVariable("WINDIR");
    if (windir.isEmpty())
        windir = QStringLiteral("C:\\Windows");
    return QDir(windir).filePath(QStringLiteral("System32/powercfg.exe"));
}

} // namespace

PowercfgReportData PowercfgReport::generate(const QString &workDir, int timeoutMs)
{
    PowercfgReportData data;

    QDir dir(workDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        data.error = QStringLiteral("Cannot create working directory %1").arg(workDir);
        return data;
    }
    const QString xmlPath = dir.filePath(QStringLiteral("faraday_batteryreport.xml"));

    QProcess proc;
    proc.setProgram(powercfgPath());
    proc.setArguments({ QStringLiteral("/batteryreport"),
                        QStringLiteral("/xml"),
                        QStringLiteral("/output"), QDir::toNativeSeparators(xmlPath) });
    proc.start(QIODevice::ReadOnly);
    if (!proc.waitForStarted(5000)) {
        data.error = QStringLiteral("powercfg failed to start: %1").arg(proc.errorString());
        return data;
    }
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(2000);
        data.error = QStringLiteral("powercfg timed out after %1 ms").arg(timeoutMs);
        return data;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        data.error = QStringLiteral("powercfg exited with code %1: %2")
                         .arg(proc.exitCode())
                         .arg(QString::fromLocal8Bit(proc.readAllStandardError()).trimmed());
        return data;
    }

    QFile file(xmlPath);
    if (!file.open(QIODevice::ReadOnly)) {
        data.error = QStringLiteral("Cannot open generated report %1").arg(xmlPath);
        return data;
    }
    const QByteArray xml = file.readAll();
    file.close();
    QFile::remove(xmlPath);

    return parse(xml);
}

PowercfgReportData PowercfgReport::parse(const QByteArray &xml)
{
    PowercfgReportData data;
    if (xml.isEmpty()) {
        data.error = QStringLiteral("Empty battery report XML");
        return data;
    }

    QXmlStreamReader reader(xml);
    bool sawRoot = false;

    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement())
            continue;

        const auto name = reader.name();

        if (name == QLatin1String("BatteryReport")) {
            sawRoot = true;
        } else if (name == QLatin1String("Battery")) {
            PowercfgReportData::BatteryInfo info;
            while (!(reader.isEndElement() && reader.name() == QLatin1String("Battery"))
                   && !reader.atEnd()) {
                reader.readNext();
                if (!reader.isStartElement())
                    continue;
                const auto field = reader.name();
                const QString text = reader.readElementText(QXmlStreamReader::SkipChildElements);
                if (field == QLatin1String("Id"))
                    info.id = text;
                else if (field == QLatin1String("Manufacturer"))
                    info.manufacturer = text;
                else if (field == QLatin1String("SerialNumber"))
                    info.serialNumber = text;
                else if (field == QLatin1String("Chemistry"))
                    info.chemistry = text;
                else if (field == QLatin1String("DesignCapacity"))
                    info.designCapacitymWh = toOptInt64(text);
                else if (field == QLatin1String("FullChargeCapacity"))
                    info.fullChargeCapacitymWh = toOptInt64(text);
                else if (field == QLatin1String("CycleCount"))
                    info.cycleCount = toOptInt64(text);
            }
            data.batteries.append(info);
        } else if (name == QLatin1String("HistoryEntry")) {
            const QXmlStreamAttributes attrs = reader.attributes();
            PowercfgReportData::HistoryEntry entry;
            entry.start = parseTimestamp(attrs.value(QLatin1String("LocalStartDate")).toString());
            if (!entry.start.isValid())
                entry.start = parseTimestamp(attrs.value(QLatin1String("StartDate")).toString());
            entry.end = parseTimestamp(attrs.value(QLatin1String("LocalEndDate")).toString());
            if (!entry.end.isValid())
                entry.end = parseTimestamp(attrs.value(QLatin1String("EndDate")).toString());
            entry.designCapacitymWh =
                toOptInt64(attrs.value(QLatin1String("DesignCapacity")).toString());
            entry.fullChargeCapacitymWh =
                toOptInt64(attrs.value(QLatin1String("FullChargeCapacity")).toString());
            entry.cycleCount = toOptInt64(attrs.value(QLatin1String("CycleCount")).toString());
            if (entry.start.isValid())
                data.history.append(entry);
        } else if (name == QLatin1String("UsageEntry")) {
            const QXmlStreamAttributes attrs = reader.attributes();
            PowercfgReportData::UsageEntry entry;
            entry.timestamp = parseTimestamp(attrs.value(QLatin1String("LocalTimestamp")).toString());
            if (!entry.timestamp.isValid())
                entry.timestamp = parseTimestamp(attrs.value(QLatin1String("Timestamp")).toString());
            entry.entryType = attrs.value(QLatin1String("EntryType")).toString();
            entry.ac = attrs.value(QLatin1String("Ac")).toString() == QLatin1String("1");
            entry.chargeCapacitymWh =
                toOptInt64(attrs.value(QLatin1String("ChargeCapacity")).toString());
            entry.dischargemWh = toOptInt64(attrs.value(QLatin1String("Discharge")).toString());
            entry.fullChargeCapacitymWh =
                toOptInt64(attrs.value(QLatin1String("FullChargeCapacity")).toString());
            entry.duration100ns =
                toOptInt64(attrs.value(QLatin1String("Duration")).toString()).value_or(0);
            if (entry.timestamp.isValid())
                data.usage.append(entry);
        }
    }

    if (reader.hasError()) {
        data.error = QStringLiteral("XML parse error at line %1: %2")
                         .arg(reader.lineNumber())
                         .arg(reader.errorString());
        return data;
    }
    if (!sawRoot) {
        data.error = QStringLiteral("Not a battery report (missing BatteryReport root)");
        return data;
    }

    data.ok = true;
    return data;
}

} // namespace faraday
