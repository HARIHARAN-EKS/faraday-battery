#pragma once

#include "acquisition/BatteryTypes.h"
#include "acquisition/PowercfgReport.h"

#include <QSqlDatabase>
#include <QString>

namespace faraday {

struct SampleRow
{
    qint64 id = 0;
    qint64 tsMs = 0;
    QString instanceName;
    std::optional<qint64> remainingmWh;
    std::optional<qint64> fullChargedmWh;
    std::optional<qint64> chargeRatemW;
    std::optional<qint64> dischargeRatemW;
    std::optional<qint64> voltagemV;
    std::optional<double> percent;
    std::optional<double> temperatureC;
    std::optional<bool> powerOnline;
    std::optional<bool> charging;
    std::optional<bool> discharging;
    std::optional<qint64> cycleCount;
};

struct CapacityHistoryRow
{
    QDate startDate;
    QDate endDate;
    std::optional<qint64> designmWh;
    std::optional<qint64> fullChargemWh;
    std::optional<qint64> cycleCount;
    QString source;   // "powercfg" or "faraday"
};

// SQLite persistence via Qt SQL (bundled QSQLITE driver — no external
// database code). One instance per thread; the connection name is unique
// per instance. Nothing throws; every method reports through lastError().
class Database
{
public:
    Database();
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool open(const QString &filePath);
    void close();
    bool isOpen() const;
    QString lastError() const { return m_lastError; }

    bool initSchema();
    int schemaVersion();

    // Ingestion
    bool upsertStatic(const BatteryDevice &device, qint64 nowMs);
    bool insertSample(const BatterySnapshot &snapshot);
    // Transaction-wrapped bulk insert (imports, seeding, benchmarks).
    // All-or-nothing: rolls back and returns false on any failure.
    bool insertSamplesBulk(const QList<BatterySnapshot> &snapshots);
    // Inserts powercfg history buckets; duplicates (same start_date+source)
    // are ignored. Returns number of newly inserted rows, -1 on error.
    int ingestCapacityHistory(const QList<PowercfgReportData::HistoryEntry> &entries,
                              const QString &source = QStringLiteral("powercfg"));
    bool insertCapacityPoint(const CapacityHistoryRow &row);

    // Sessions
    qint64 beginSession(qint64 nowMs, std::optional<qint64> originRemainingmWh,
                        const QString &note = QString());
    bool endSession(qint64 sessionId, qint64 nowMs);

    // Queries
    QList<SampleRow> samplesBetween(qint64 fromMs, qint64 toMs,
                                    const QString &instanceName = QString(),
                                    int limit = 100000);
    QList<CapacityHistoryRow> capacityHistory();
    qint64 sampleCount();

    // settings table is reserved for DB metadata (schema_version);
    // user preferences live in the app-local settings file.
    bool setMeta(const QString &key, const QString &value);
    QString meta(const QString &key);

private:
    bool exec(const QString &sql);

    QSqlDatabase db() const;
    QString m_connectionName;
    QString m_lastError;
};

} // namespace faraday
