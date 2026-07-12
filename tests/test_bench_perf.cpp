#include <QtTest/QtTest>

#include "acquisition/BatteryReader.h"
#include "acquisition/PowercfgReport.h"
#include "data/Database.h"

#include <QElapsedTimer>
#include <QTemporaryDir>
#include <algorithm>

using namespace faraday;

// Performance benchmarks with hard gates. The gates are deliberately loose
// (10x headroom over measured values) — they exist to catch order-of-
// magnitude regressions, not to flake on a busy machine. Measured numbers
// are printed for the T4 report.
class TestBenchPerf : public QObject
{
    Q_OBJECT

    static BatterySnapshot sampleSnapshot(int i)
    {
        BatterySnapshot snap;
        snap.timestamp = QDateTime(QDate(2026, 1, 1), QTime(0, 0)).addSecs(i * 30);
        snap.batteryPresent = true;
        BatteryDevice dev;
        dev.instanceName = QStringLiteral("ACPI\\PNP0C0A\\1_0");
        dev.remainingCapacitymWh = 30000 + (i % 10000);
        dev.fullChargedCapacitymWh = 42581;
        dev.designedCapacitymWh = 42581;
        dev.chargeRatemW = 0;
        dev.dischargeRatemW = 8000;
        dev.voltagemV = 12500;
        dev.powerOnline = false;
        dev.charging = false;
        dev.discharging = true;
        snap.batteries.append(dev);
        snap.temperatureC = 35.0;
        return snap;
    }

private slots:
    void wmiAcquisitionLatency()
    {
        BatteryReader reader;
        if (!reader.initialize())
            QSKIP("WMI unavailable");
        // Warm-up read (COM/provider spin-up is not steady-state).
        reader.read();

        QVector<qint64> ms;
        for (int i = 0; i < 30; ++i) {
            QElapsedTimer t;
            t.start();
            const BatterySnapshot snap = reader.read();
            ms.append(t.elapsed());
            QVERIFY(snap.timestamp.isValid());
        }
        std::sort(ms.begin(), ms.end());
        const qint64 sum = std::accumulate(ms.begin(), ms.end(), 0ll);
        const qint64 mean = sum / ms.size();
        const qint64 p95 = ms.at(int(ms.size() * 95 / 100));
        const qint64 max = ms.last();
        qInfo("BENCH wmi_read_ms mean=%lld p95=%lld max=%lld n=30",
              qlonglong(mean), qlonglong(p95), qlonglong(max));
        // Gate: a full multi-class WMI read must stay well under a second on
        // steady state — 2 s here means the provider path regressed badly.
        QVERIFY2(mean < 2000, qPrintable(QStringLiteral("mean=%1ms").arg(mean)));
    }

    void powercfgGenerateAndParseLatency()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QElapsedTimer t;
        t.start();
        const PowercfgReportData data = PowercfgReport::generate(dir.path());
        const qint64 ms = t.elapsed();
        if (!data.ok)
            QSKIP("powercfg unavailable");
        qInfo("BENCH powercfg_generate_parse_ms=%lld usage=%lld history=%lld",
              qlonglong(ms), qlonglong(data.usage.size()), qlonglong(data.history.size()));
        QVERIFY2(ms < 30000, qPrintable(QStringLiteral("%1ms").arg(ms)));
    }

    void sqliteInsertThroughputAndGrowth()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = QDir(dir.path()).filePath(QStringLiteral("bench.sqlite"));
        Database db;
        QVERIFY(db.open(path));
        QVERIFY(db.initSchema());
        const qint64 emptyBytes = QFileInfo(path).size();

        // (a) Autocommit path — what the live sampler actually does, one
        // durable insert per tick.
        const int n = 500;
        QElapsedTimer t;
        t.start();
        for (int i = 0; i < n; ++i)
            QVERIFY(db.insertSample(sampleSnapshot(i)));
        const qint64 ms = t.elapsed();

        // (b) Transaction-wrapped bulk path (imports/seeding).
        const int bulkN = 20000;
        QList<BatterySnapshot> bulk;
        bulk.reserve(bulkN);
        for (int i = 0; i < bulkN; ++i)
            bulk.append(sampleSnapshot(n + i));
        t.restart();
        QVERIFY(db.insertSamplesBulk(bulk));
        const qint64 bulkMs = t.elapsed();
        QCOMPARE(db.sampleCount(), qint64(n + bulkN));
        db.close();

        const qint64 bytes = QFileInfo(path).size();
        const double rowsPerSec = n / (ms / 1000.0);
        const double bulkRowsPerSec = bulkN / (bulkMs / 1000.0);
        const double bytesPerRow = double(bytes - emptyBytes) / (n + bulkN);
        qInfo("BENCH sqlite_insert autocommit rows=%d ms=%lld rows_per_sec=%.0f | "
              "bulk rows=%d ms=%lld rows_per_sec=%.0f | bytes_per_row=%.1f db_bytes=%lld",
              n, qlonglong(ms), rowsPerSec, bulkN, qlonglong(bulkMs), bulkRowsPerSec,
              bytesPerRow, qlonglong(bytes));
        // Growth projections for the report (per configured interval).
        for (const int interval : { 1, 30, 300 }) {
            const double rowsPerDay = 86400.0 / interval;
            qInfo("BENCH db_growth interval=%ds mb_per_day=%.2f mb_30d=%.1f mb_365d=%.1f",
                  interval, rowsPerDay * bytesPerRow / 1e6,
                  rowsPerDay * bytesPerRow * 30 / 1e6, rowsPerDay * bytesPerRow * 365 / 1e6);
        }
        // Gate: a sampler tick must never be starved by its own insert.
        QVERIFY2(rowsPerSec > 50, qPrintable(QStringLiteral("%1 rows/s").arg(rowsPerSec)));
    }

    void queryLatencyByDbSize_data()
    {
        QTest::addColumn<int>("rows");
        QTest::addRow("1k") << 1000;
        QTest::addRow("10k") << 10000;
        QTest::addRow("100k") << 100000;
        // The 1M case takes ~a minute to seed; run explicitly for the perf
        // report via FARADAY_BENCH_FULL=1 rather than on every CI pass.
        if (qEnvironmentVariableIsSet("FARADAY_BENCH_FULL"))
            QTest::addRow("1M") << 1000000;
    }
    void queryLatencyByDbSize()
    {
        QFETCH(int, rows);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = QDir(dir.path()).filePath(QStringLiteral("bench.sqlite"));
        Database db;
        QVERIFY(db.open(path));
        QVERIFY(db.initSchema());
        const int chunk = 50000;
        for (int base = 0; base < rows; base += chunk) {
            QList<BatterySnapshot> batch;
            batch.reserve(qMin(chunk, rows - base));
            for (int i = base; i < qMin(base + chunk, rows); ++i)
                batch.append(sampleSnapshot(i));
            QVERIFY(db.insertSamplesBulk(batch));
        }

        // The two live query shapes: bounded window (monitor/export) and
        // full count (dashboard sampling card).
        QElapsedTimer t;
        t.start();
        const auto window = db.samplesBetween(
            QDateTime(QDate(2026, 1, 1), QTime(0, 0)).toMSecsSinceEpoch(),
            QDateTime(QDate(2026, 1, 1), QTime(0, 0)).addSecs(30ll * rows).toMSecsSinceEpoch(),
            QString(), 100000);
        const qint64 windowMs = t.elapsed();
        t.restart();
        const qint64 count = db.sampleCount();
        const qint64 countMs = t.elapsed();
        QCOMPARE(count, qint64(rows));
        QCOMPARE(window.size(), qMin(rows, 100000));
        qInfo("BENCH query rows=%d samplesBetween_ms=%lld sampleCount_ms=%lld",
              rows, qlonglong(windowMs), qlonglong(countMs));
        // Gate: indexed range scan; 100k rows in >5s would mean the index died.
        QVERIFY2(windowMs < 5000, qPrintable(QStringLiteral("%1ms").arg(windowMs)));
        QVERIFY2(countMs < 2000, qPrintable(QStringLiteral("%1ms").arg(countMs)));
        db.close();
    }
};

QTEST_GUILESS_MAIN(TestBenchPerf)
#include "test_bench_perf.moc"
