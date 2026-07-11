#pragma once

#include "data/Database.h"

#include <QString>

namespace faraday {

// Report/exports input, decoupled from live objects so it is fully testable.
struct ReportInput
{
    QString appVersion;
    QDateTime generatedAt;

    QString batteryName;
    QString manufacturer;
    QString chemistry;
    QString serialNumber;

    double healthPercent = -1;
    double wearPercent = -1;
    int cycleCount = -1;
    qint64 designmWh = -1;
    qint64 fullChargemWh = -1;

    QString gradeName;
    QString gradeColor;
    QString verdictHeadline;
    QStringList verdictDetails;
    QStringList insights;

    QList<CapacityHistoryRow> history;
};

class Exporter
{
public:
    // Raw sample exports. Return true on success; error via errorOut.
    static bool exportCsv(const QList<SampleRow> &samples, const QString &filePath,
                          QString *errorOut = nullptr);
    static bool exportJson(const QList<SampleRow> &samples, const QString &filePath,
                           QString *errorOut = nullptr);

    // Self-contained HTML health report: inline CSS + inline SVG chart,
    // zero external references.
    static QString buildHtmlReport(const ReportInput &input);
    static bool writeHtmlReport(const ReportInput &input, const QString &filePath,
                                QString *errorOut = nullptr);
};

} // namespace faraday
