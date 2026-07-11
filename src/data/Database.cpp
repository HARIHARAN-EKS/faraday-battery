#include "Database.h"

#include "core/Metrics.h"

#include <QAtomicInt>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace faraday {

namespace {

QAtomicInt g_connectionCounter;

QVariant optVariant(const std::optional<quint32> &v)
{
    return v.has_value() ? QVariant(static_cast<qint64>(*v)) : QVariant();
}

QVariant optVariant(const std::optional<qint32> &v)
{
    return v.has_value() ? QVariant(static_cast<qint64>(*v)) : QVariant();
}

QVariant optVariant(const std::optional<qint64> &v)
{
    return v.has_value() ? QVariant(*v) : QVariant();
}

QVariant optVariant(const std::optional<double> &v)
{
    return v.has_value() ? QVariant(*v) : QVariant();
}

QVariant optVariant(const std::optional<bool> &v)
{
    return v.has_value() ? QVariant(*v ? 1 : 0) : QVariant();
}

std::optional<qint64> optInt64(const QVariant &v)
{
    if (v.isNull() || !v.isValid())
        return std::nullopt;
    bool ok = false;
    const qint64 x = v.toLongLong(&ok);
    if (!ok)
        return std::nullopt;
    return x;
}

std::optional<double> optDouble(const QVariant &v)
{
    if (v.isNull() || !v.isValid())
        return std::nullopt;
    bool ok = false;
    const double x = v.toDouble(&ok);
    if (!ok)
        return std::nullopt;
    return x;
}

std::optional<bool> optBool(const QVariant &v)
{
    if (v.isNull() || !v.isValid())
        return std::nullopt;
    return v.toInt() != 0;
}

} // namespace

Database::Database()
    : m_connectionName(QStringLiteral("faraday_db_%1")
                           .arg(g_connectionCounter.fetchAndAddRelaxed(1)))
{
}

Database::~Database()
{
    close();
}

QSqlDatabase Database::db() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

bool Database::open(const QString &filePath)
{
    close();
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(filePath);
    if (!database.open()) {
        m_lastError = database.lastError().text();
        QSqlDatabase::removeDatabase(m_connectionName);
        return false;
    }
    m_lastError.clear();
    return true;
}

void Database::close()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase database = db();
            if (database.isOpen())
                database.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName) && db().isOpen();
}

bool Database::exec(const QString &sql)
{
    QSqlQuery query(db());
    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::initSchema()
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("initSchema() on a closed database");
        return false;
    }

    const char *statements[] = {
        "CREATE TABLE IF NOT EXISTS battery_static ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " instance_name TEXT NOT NULL UNIQUE,"
        " name TEXT, manufacturer TEXT, serial_number TEXT, chemistry TEXT,"
        " design_capacity_mwh INTEGER, design_voltage_mv INTEGER,"
        " first_seen_ms INTEGER NOT NULL, last_seen_ms INTEGER NOT NULL)",

        "CREATE TABLE IF NOT EXISTS samples ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " ts_ms INTEGER NOT NULL, instance_name TEXT NOT NULL,"
        " remaining_mwh INTEGER, full_charged_mwh INTEGER,"
        " charge_rate_mw INTEGER, discharge_rate_mw INTEGER,"
        " voltage_mv INTEGER, percent REAL, temperature_c REAL,"
        " power_online INTEGER, charging INTEGER, discharging INTEGER,"
        " cycle_count INTEGER)",

        "CREATE INDEX IF NOT EXISTS idx_samples_ts ON samples(ts_ms)",
        "CREATE INDEX IF NOT EXISTS idx_samples_instance ON samples(instance_name, ts_ms)",

        "CREATE TABLE IF NOT EXISTS capacity_history ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " start_date TEXT NOT NULL, end_date TEXT,"
        " design_mwh INTEGER, full_charge_mwh INTEGER, cycle_count INTEGER,"
        " source TEXT NOT NULL,"
        " UNIQUE(start_date, source))",

        "CREATE TABLE IF NOT EXISTS sessions ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " started_ms INTEGER NOT NULL, ended_ms INTEGER,"
        " origin_remaining_mwh INTEGER, note TEXT)",

        "CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)",
    };
    for (const char *sql : statements) {
        if (!exec(QLatin1String(sql)))
            return false;
    }
    if (meta(QStringLiteral("schema_version")).isEmpty())
        return setMeta(QStringLiteral("schema_version"), QStringLiteral("1"));
    return true;
}

int Database::schemaVersion()
{
    return meta(QStringLiteral("schema_version")).toInt();
}

bool Database::setMeta(const QString &key, const QString &value)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) VALUES(?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

QString Database::meta(const QString &key)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return QString();
    }
    if (query.next())
        return query.value(0).toString();
    return QString();
}

bool Database::upsertStatic(const BatteryDevice &device, qint64 nowMs)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO battery_static(instance_name, name, manufacturer, serial_number,"
        " chemistry, design_capacity_mwh, design_voltage_mv, first_seen_ms, last_seen_ms) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(instance_name) DO UPDATE SET"
        " name = excluded.name, manufacturer = excluded.manufacturer,"
        " serial_number = excluded.serial_number, chemistry = excluded.chemistry,"
        " design_capacity_mwh = COALESCE(excluded.design_capacity_mwh, design_capacity_mwh),"
        " design_voltage_mv = COALESCE(excluded.design_voltage_mv, design_voltage_mv),"
        " last_seen_ms = excluded.last_seen_ms"));
    query.addBindValue(device.instanceName);
    query.addBindValue(device.name);
    query.addBindValue(device.manufacturer);
    query.addBindValue(device.serialNumber);
    query.addBindValue(device.chemistry);
    query.addBindValue(optVariant(device.designedCapacitymWh));
    query.addBindValue(optVariant(device.designVoltagemV));
    query.addBindValue(nowMs);
    query.addBindValue(nowMs);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool Database::insertSample(const BatterySnapshot &snapshot)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("insertSample() on a closed database");
        return false;
    }
    const qint64 tsMs = snapshot.timestamp.toMSecsSinceEpoch();
    for (const BatteryDevice &device : snapshot.batteries) {
        QSqlQuery query(db());
        query.prepare(QStringLiteral(
            "INSERT INTO samples(ts_ms, instance_name, remaining_mwh, full_charged_mwh,"
            " charge_rate_mw, discharge_rate_mw, voltage_mv, percent, temperature_c,"
            " power_online, charging, discharging, cycle_count) "
            "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
        query.addBindValue(tsMs);
        query.addBindValue(device.instanceName);
        query.addBindValue(optVariant(device.remainingCapacitymWh));
        query.addBindValue(optVariant(device.fullChargedCapacitymWh));
        query.addBindValue(optVariant(device.chargeRatemW));
        query.addBindValue(optVariant(device.dischargeRatemW));
        query.addBindValue(optVariant(device.voltagemV));

        std::optional<double> percent = metrics::computedChargePercent(
            device.remainingCapacitymWh, device.fullChargedCapacitymWh);
        if (!percent.has_value() && device.estimatedChargeRemainingPct.has_value())
            percent = static_cast<double>(*device.estimatedChargeRemainingPct);
        query.addBindValue(optVariant(percent));
        query.addBindValue(optVariant(snapshot.temperatureC));
        query.addBindValue(optVariant(device.powerOnline));
        query.addBindValue(optVariant(device.charging));
        query.addBindValue(optVariant(device.discharging));
        query.addBindValue(optVariant(device.cycleCount));
        if (!query.exec()) {
            m_lastError = query.lastError().text();
            return false;
        }
    }
    return true;
}

int Database::ingestCapacityHistory(const QList<PowercfgReportData::HistoryEntry> &entries,
                                    const QString &source)
{
    if (!isOpen()) {
        m_lastError = QStringLiteral("ingestCapacityHistory() on a closed database");
        return -1;
    }
    int inserted = 0;
    for (const auto &entry : entries) {
        if (!entry.start.isValid())
            continue;
        QSqlQuery query(db());
        query.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO capacity_history(start_date, end_date, design_mwh,"
            " full_charge_mwh, cycle_count, source) VALUES(?, ?, ?, ?, ?, ?)"));
        query.addBindValue(entry.start.date().toString(Qt::ISODate));
        query.addBindValue(entry.end.isValid() ? QVariant(entry.end.date().toString(Qt::ISODate))
                                               : QVariant());
        query.addBindValue(optVariant(entry.designCapacitymWh));
        query.addBindValue(optVariant(entry.fullChargeCapacitymWh));
        query.addBindValue(optVariant(entry.cycleCount));
        query.addBindValue(source);
        if (!query.exec()) {
            m_lastError = query.lastError().text();
            return -1;
        }
        inserted += query.numRowsAffected() > 0 ? 1 : 0;
    }
    return inserted;
}

bool Database::insertCapacityPoint(const CapacityHistoryRow &row)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO capacity_history(start_date, end_date, design_mwh,"
        " full_charge_mwh, cycle_count, source) VALUES(?, ?, ?, ?, ?, ?)"));
    query.addBindValue(row.startDate.toString(Qt::ISODate));
    query.addBindValue(row.endDate.isValid() ? QVariant(row.endDate.toString(Qt::ISODate))
                                             : QVariant());
    query.addBindValue(optVariant(row.designmWh));
    query.addBindValue(optVariant(row.fullChargemWh));
    query.addBindValue(optVariant(row.cycleCount));
    query.addBindValue(row.source.isEmpty() ? QStringLiteral("faraday") : row.source);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

qint64 Database::beginSession(qint64 nowMs, std::optional<qint64> originRemainingmWh,
                              const QString &note)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral(
        "INSERT INTO sessions(started_ms, origin_remaining_mwh, note) VALUES(?, ?, ?)"));
    query.addBindValue(nowMs);
    query.addBindValue(optVariant(originRemainingmWh));
    query.addBindValue(note);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toLongLong();
}

bool Database::endSession(qint64 sessionId, qint64 nowMs)
{
    QSqlQuery query(db());
    query.prepare(QStringLiteral("UPDATE sessions SET ended_ms = ? WHERE id = ?"));
    query.addBindValue(nowMs);
    query.addBindValue(sessionId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

QList<SampleRow> Database::samplesBetween(qint64 fromMs, qint64 toMs,
                                          const QString &instanceName, int limit)
{
    QList<SampleRow> rows;
    QSqlQuery query(db());
    QString sql = QStringLiteral(
        "SELECT id, ts_ms, instance_name, remaining_mwh, full_charged_mwh, charge_rate_mw,"
        " discharge_rate_mw, voltage_mv, percent, temperature_c, power_online, charging,"
        " discharging, cycle_count FROM samples WHERE ts_ms BETWEEN ? AND ?");
    if (!instanceName.isEmpty())
        sql += QStringLiteral(" AND instance_name = ?");
    sql += QStringLiteral(" ORDER BY ts_ms ASC LIMIT ?");
    query.prepare(sql);
    query.addBindValue(fromMs);
    query.addBindValue(toMs);
    if (!instanceName.isEmpty())
        query.addBindValue(instanceName);
    query.addBindValue(limit);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return rows;
    }
    while (query.next()) {
        SampleRow row;
        row.id = query.value(0).toLongLong();
        row.tsMs = query.value(1).toLongLong();
        row.instanceName = query.value(2).toString();
        row.remainingmWh = optInt64(query.value(3));
        row.fullChargedmWh = optInt64(query.value(4));
        row.chargeRatemW = optInt64(query.value(5));
        row.dischargeRatemW = optInt64(query.value(6));
        row.voltagemV = optInt64(query.value(7));
        row.percent = optDouble(query.value(8));
        row.temperatureC = optDouble(query.value(9));
        row.powerOnline = optBool(query.value(10));
        row.charging = optBool(query.value(11));
        row.discharging = optBool(query.value(12));
        row.cycleCount = optInt64(query.value(13));
        rows.append(row);
    }
    return rows;
}

QList<CapacityHistoryRow> Database::capacityHistory()
{
    QList<CapacityHistoryRow> rows;
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral(
            "SELECT start_date, end_date, design_mwh, full_charge_mwh, cycle_count, source"
            " FROM capacity_history ORDER BY start_date ASC"))) {
        m_lastError = query.lastError().text();
        return rows;
    }
    while (query.next()) {
        CapacityHistoryRow row;
        row.startDate = QDate::fromString(query.value(0).toString(), Qt::ISODate);
        row.endDate = QDate::fromString(query.value(1).toString(), Qt::ISODate);
        row.designmWh = optInt64(query.value(2));
        row.fullChargemWh = optInt64(query.value(3));
        row.cycleCount = optInt64(query.value(4));
        row.source = query.value(5).toString();
        rows.append(row);
    }
    return rows;
}

qint64 Database::sampleCount()
{
    QSqlQuery query(db());
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM samples"))) {
        m_lastError = query.lastError().text();
        return -1;
    }
    return query.next() ? query.value(0).toLongLong() : -1;
}

} // namespace faraday
