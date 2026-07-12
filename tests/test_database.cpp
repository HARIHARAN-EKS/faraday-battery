#include <QtTest/QtTest>

#include "data/Database.h"

using namespace faraday;

namespace {

BatterySnapshot makeSnapshot(qint64 tsMs, quint32 remaining, quint32 full)
{
    BatterySnapshot snap;
    snap.timestamp = QDateTime::fromMSecsSinceEpoch(tsMs);
    snap.batteryPresent = true;
    BatteryDevice dev;
    dev.instanceName = QStringLiteral("ACPI\\PNP0C0A\\1_0");
    dev.remainingCapacitymWh = remaining;
    dev.fullChargedCapacitymWh = full;
    dev.chargeRatemW = 0;
    dev.dischargeRatemW = 8000;
    dev.voltagemV = 12686;
    dev.powerOnline = false;
    dev.charging = false;
    dev.discharging = true;
    dev.cycleCount = 35;
    snap.batteries.append(dev);
    snap.temperatureC = 32.5;
    return snap;
}

} // namespace

class TestDatabase : public QObject
{
    Q_OBJECT

    QString m_dir;

private slots:
    void initTestCase()
    {
        m_dir = QDir::temp().filePath(QStringLiteral("faraday_test_db"));
        QDir().mkpath(m_dir);
    }

    void opensAndCreatesSchema()
    {
        Database db;
        QVERIFY2(db.open(QDir(m_dir).filePath(QStringLiteral("schema.sqlite"))),
                 qPrintable(db.lastError()));
        QVERIFY2(db.initSchema(), qPrintable(db.lastError()));
        QCOMPARE(db.schemaVersion(), 1);
        QVERIFY(db.initSchema()); // idempotent
    }

    void failsCleanlyWhenClosed()
    {
        Database db;
        QVERIFY(!db.isOpen());
        QVERIFY(!db.initSchema());
        QVERIFY(!db.insertSample(makeSnapshot(1000, 1, 2)));
        QCOMPARE(db.ingestCapacityHistory({}), -1);
        QVERIFY(!db.lastError().isEmpty());
    }

    void insertsAndQueriesSamples()
    {
        Database db;
        QVERIFY(db.open(QDir(m_dir).filePath(QStringLiteral("samples.sqlite"))));
        QVERIFY(db.initSchema());

        QVERIFY2(db.insertSample(makeSnapshot(1000, 32288, 42401)), qPrintable(db.lastError()));
        QVERIFY(db.insertSample(makeSnapshot(2000, 32000, 42401)));
        QVERIFY(db.insertSample(makeSnapshot(3000, 31700, 42401)));
        QCOMPARE(db.sampleCount(), qint64(3));

        const auto rows = db.samplesBetween(1500, 2500);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().tsMs, qint64(2000));
        QCOMPARE(rows.first().remainingmWh.value_or(0), qint64(32000));
        QCOMPARE(rows.first().dischargeRatemW.value_or(0), qint64(8000));
        QVERIFY(qAbs(rows.first().percent.value_or(0) - 75.47) < 0.01);
        QCOMPARE(rows.first().temperatureC.value_or(0), 32.5);
        QCOMPARE(rows.first().discharging.value_or(false), true);
        QCOMPARE(rows.first().powerOnline.value_or(true), false);

        // Instance filter
        QCOMPARE(db.samplesBetween(0, 5000, QStringLiteral("nope")).size(), 0);
        QCOMPARE(db.samplesBetween(0, 5000, QStringLiteral("ACPI\\PNP0C0A\\1_0")).size(), 3);
    }

    void upsertsStaticData()
    {
        Database db;
        QVERIFY(db.open(QDir(m_dir).filePath(QStringLiteral("static.sqlite"))));
        QVERIFY(db.initSchema());

        BatteryDevice dev;
        dev.instanceName = QStringLiteral("ACPI\\PNP0C0A\\1_0");
        dev.manufacturer = QStringLiteral("ACME Corp");
        dev.designedCapacitymWh = 42401;
        QVERIFY2(db.upsertStatic(dev, 1000), qPrintable(db.lastError()));

        // Second upsert without design capacity must not clobber it (COALESCE).
        BatteryDevice dev2;
        dev2.instanceName = dev.instanceName;
        dev2.manufacturer = QStringLiteral("HP");
        QVERIFY(db.upsertStatic(dev2, 2000));
    }

    void ingestsHistoryWithoutDuplicates()
    {
        Database db;
        QVERIFY(db.open(QDir(m_dir).filePath(QStringLiteral("history.sqlite"))));
        QVERIFY(db.initSchema());

        QList<PowercfgReportData::HistoryEntry> entries;
        PowercfgReportData::HistoryEntry e1;
        e1.start = QDateTime(QDate(2026, 6, 1), QTime(0, 0));
        e1.end = QDateTime(QDate(2026, 6, 8), QTime(0, 0));
        e1.designCapacitymWh = 42413;
        e1.fullChargeCapacitymWh = 42413;
        e1.cycleCount = 34;
        PowercfgReportData::HistoryEntry e2 = e1;
        e2.start = QDateTime(QDate(2026, 6, 8), QTime(0, 0));
        e2.fullChargeCapacitymWh = 42000;
        entries << e1 << e2;

        QCOMPARE(db.ingestCapacityHistory(entries), 2);
        QCOMPARE(db.ingestCapacityHistory(entries), 0); // idempotent re-ingest

        const auto rows = db.capacityHistory();
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows.first().startDate, QDate(2026, 6, 1));
        QCOMPARE(rows.first().fullChargemWh.value_or(0), qint64(42413));
        QCOMPARE(rows.last().fullChargemWh.value_or(0), qint64(42000));
        QCOMPARE(rows.first().source, QStringLiteral("powercfg"));

        // Manual capacity point with a different source coexists.
        CapacityHistoryRow manual;
        manual.startDate = QDate(2026, 6, 1);
        manual.fullChargemWh = 42350;
        manual.source = QStringLiteral("faraday");
        QVERIFY(db.insertCapacityPoint(manual));
        QCOMPARE(db.capacityHistory().size(), 3);
    }

    void sessionLifecycle()
    {
        Database db;
        QVERIFY(db.open(QDir(m_dir).filePath(QStringLiteral("sessions.sqlite"))));
        QVERIFY(db.initSchema());

        const qint64 id = db.beginSession(5000, qint64(32288), QStringLiteral("monitor"));
        QVERIFY(id > 0);
        QVERIFY(db.endSession(id, 9000));
        QVERIFY(!db.endSession(id + 999, 9000)); // unknown session
    }

    void metaRoundTrip()
    {
        Database db;
        QVERIFY(db.open(QDir(m_dir).filePath(QStringLiteral("meta.sqlite"))));
        QVERIFY(db.initSchema());
        QVERIFY(db.setMeta(QStringLiteral("k"), QStringLiteral("v1")));
        QCOMPARE(db.meta(QStringLiteral("k")), QStringLiteral("v1"));
        QVERIFY(db.setMeta(QStringLiteral("k"), QStringLiteral("v2")));
        QCOMPARE(db.meta(QStringLiteral("k")), QStringLiteral("v2"));
        QVERIFY(db.meta(QStringLiteral("missing")).isEmpty());
    }

    void cleanupTestCase()
    {
        QDir(m_dir).removeRecursively();
    }
};

QTEST_GUILESS_MAIN(TestDatabase)
#include "test_database.moc"
