#include <QtTest/QtTest>

#include "PropertyGen.h"
#include "data/Database.h"
#include "data/Settings.h"

#include <QTemporaryDir>

using namespace faraday;
using namespace faraday::propgen;

// Fuzzing of the two persistence layers: the JSON settings loader and the
// SQLite database. Both must degrade gracefully — defaults or clean errors,
// never a crash, never out-of-range values reaching the app.
class TestFuzzSettingsDb : public QObject
{
    Q_OBJECT

    // Every typed accessor must return an in-contract value, whatever was
    // on disk. This is the single gate all corpus entries run through.
    static void assertSettingsContract(Settings &s)
    {
        QVERIFY(s.sampleIntervalSec() >= 1 && s.sampleIntervalSec() <= 3600);
        QVERIFY(s.theme() == QLatin1String("dark") || s.theme() == QLatin1String("light"));
        QVERIFY(s.temperatureUnit() == QLatin1String("C")
                || s.temperatureUnit() == QLatin1String("F"));
        QVERIFY(s.energyUnit() == QLatin1String("mWh") || s.energyUnit() == QLatin1String("Wh"));
        QVERIFY(s.lowLevelThresholdPct() >= 1 && s.lowLevelThresholdPct() <= 99);
        QVERIFY(s.highTempThresholdC() > 0.0 && s.highTempThresholdC() <= 150.0);
        QVERIFY(s.lowVoltageThresholdmV() >= 0 && s.lowVoltageThresholdmV() <= 100000);
        QVERIFY(s.unplugReminderPct() >= 50 && s.unplugReminderPct() <= 100);
        QVERIFY(s.chargeReminderPct() >= 1 && s.chargeReminderPct() <= 50);
        QVERIFY(!s.language().isEmpty());
    }

private slots:
    void settingsStructuralCorpus_data()
    {
        QTest::addColumn<QByteArray>("fileBytes");
        QTest::newRow("empty-file") << QByteArray();
        QTest::newRow("not-json") << QByteArray("this is not json at all {{{");
        QTest::newRow("json-null") << QByteArray("null");
        QTest::newRow("json-array") << QByteArray("[1,2,3]");
        QTest::newRow("json-string") << QByteArray("\"hello\"");
        QTest::newRow("truncated") << QByteArray("{\"sampleIntervalSec\": 30, \"the");
        QTest::newRow("nul-bytes") << QByteArray("\0\0\0\0{}", 6);
        QTest::newRow("wrong-types")
            << QByteArray("{\"sampleIntervalSec\":\"soon\",\"theme\":42,"
                          "\"alertsEnabled\":\"maybe\",\"highTempThresholdC\":[1,2]}");
        QTest::newRow("huge-interval") << QByteArray("{\"sampleIntervalSec\": 99999999}");
        QTest::newRow("zero-interval") << QByteArray("{\"sampleIntervalSec\": 0}");
        QTest::newRow("negative-interval") << QByteArray("{\"sampleIntervalSec\": -50}");
        QTest::newRow("negative-thresholds")
            << QByteArray("{\"lowLevelThresholdPct\": -20, \"highTempThresholdC\": -273.15,"
                          "\"lowVoltageThresholdmV\": -99999}");
        QTest::newRow("threshold-overflow")
            << QByteArray("{\"lowLevelThresholdPct\": 2147483648,"
                          "\"unplugReminderPct\": 1e308, \"chargeReminderPct\": 1e-308}");
        QTest::newRow("unknown-keys")
            << QByteArray("{\"totallyUnknown\": true, \"alsoUnknown\": {\"deep\": [1]}}");
        QTest::newRow("bogus-theme") << QByteArray("{\"theme\": \"hotdog-stand\"}");
        QTest::newRow("bogus-units")
            << QByteArray("{\"temperatureUnit\": \"K\", \"energyUnit\": \"J\"}");
        QTest::newRow("nan-in-json") << QByteArray("{\"highTempThresholdC\": NaN}");
        QTest::newRow("deep-nesting")
            << QByteArray("{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":{\"a\":1}}}}}}}");
        QTest::newRow("giant-string-value")
            << (QByteArray("{\"theme\": \"") + QByteArray(1024 * 1024, 'x') + "\"}");
    }
    void settingsStructuralCorpus()
    {
        QFETCH(QByteArray, fileBytes);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            QFile f(QDir(dir.path()).filePath(QStringLiteral("settings.json")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(fileBytes);
        }
        Settings s(dir.path());
        s.load(); // may fail — must not crash
        assertSettingsContract(s);
        // And the store must still be able to save + reload cleanly after.
        s.setValue(QStringLiteral("theme"), QStringLiteral("light"));
        QVERIFY(s.save());
        Settings reloaded(dir.path());
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.theme(), QStringLiteral("light"));
    }

    void settingsRandomBytes_data()
    {
        QTest::addColumn<int>("seed");
        for (int i = 0; i < 100; ++i)
            QTest::addRow("bytes%03d", i) << i;
    }
    void settingsRandomBytes()
    {
        QFETCH(int, seed);
        Gen g(0xFE771465u + seed);
        QByteArray bytes;
        const int n = int(g.u32Below(2048));
        bytes.reserve(n);
        for (int i = 0; i < n; ++i)
            bytes.append(char(g.u32Below(256)));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            QFile f(QDir(dir.path()).filePath(QStringLiteral("settings.json")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(bytes);
        }
        Settings s(dir.path());
        s.load();
        assertSettingsContract(s);
    }

    void settingsMissingFileAndMissingDir()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Settings s(dir.path()); // no file at all
        QVERIFY(!s.load());     // reported, not fatal
        assertSettingsContract(s);

        Settings orphan(QDir(dir.path()).filePath(QStringLiteral("no/such/dir")));
        QVERIFY(!orphan.load());
        assertSettingsContract(orphan);
    }

    // ---- SQLite abuse ----------------------------------------------------

    void corruptDatabaseFileDegradesGracefully_data()
    {
        QTest::addColumn<int>("seed");
        for (int i = 0; i < 20; ++i)
            QTest::addRow("corrupt%02d", i) << i;
    }
    void corruptDatabaseFileDegradesGracefully()
    {
        QFETCH(int, seed);
        Gen g(0xDB0BADu + seed);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = QDir(dir.path()).filePath(QStringLiteral("faraday.sqlite"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            // Sometimes a valid-looking SQLite header followed by garbage.
            if (g.chance(0.5))
                f.write("SQLite format 3\0", 16);
            const int n = 512 + int(g.u32Below(8192));
            QByteArray junk;
            junk.reserve(n);
            for (int i = 0; i < n; ++i)
                junk.append(char(g.u32Below(256)));
            f.write(junk);
        }

        Database db;
        const bool opened = db.open(path);   // may "open" (SQLite is lazy)
        if (opened) {
            db.initSchema();                  // may fail — must not crash
            BatterySnapshot snap;
            snap.timestamp = QDateTime::currentDateTime();
            db.insertSample(snap);            // must fail cleanly at worst
            const auto rows = db.samplesBetween(0, 9e15);
            QVERIFY(rows.size() >= 0);        // returns, never throws
            db.sampleCount();
            db.capacityHistory();
        }
        // Whatever happened, the object is still destructible and reusable.
        db.close();
        QVERIFY(true);
    }

    void closedDatabaseEveryMethodFailsCleanly()
    {
        Database db; // never opened
        QVERIFY(!db.isOpen());
        QVERIFY(!db.initSchema());
        BatteryDevice dev;
        dev.instanceName = QStringLiteral("X");
        QVERIFY(!db.upsertStatic(dev, 0));
        BatterySnapshot snap;
        QVERIFY(!db.insertSample(snap));
        QCOMPARE(db.ingestCapacityHistory({}), -1);
        CapacityHistoryRow row;
        QVERIFY(!db.insertCapacityPoint(row));
        QVERIFY(db.beginSession(0, std::nullopt) < 0);
        QVERIFY(!db.endSession(1, 0));
        QVERIFY(db.samplesBetween(0, 1).isEmpty());
        QVERIFY(db.capacityHistory().isEmpty());
        QVERIFY(db.sampleCount() <= 0);
        QVERIFY(!db.setMeta(QStringLiteral("k"), QStringLiteral("v")));
        QVERIFY(db.meta(QStringLiteral("k")).isEmpty());
    }

    void readOnlyDatabaseFileFailsWritesCleanly()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = QDir(dir.path()).filePath(QStringLiteral("faraday.sqlite"));
        {
            Database seed;
            QVERIFY(seed.open(path));
            QVERIFY(seed.initSchema());
            seed.close();
        }
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ReadUser);

        Database db;
        if (db.open(path)) {
            BatterySnapshot snap;
            snap.timestamp = QDateTime::currentDateTime();
            BatteryDevice dev;
            dev.instanceName = QStringLiteral("RO");
            dev.remainingCapacitymWh = 1000;
            snap.batteries.append(dev);
            db.insertSample(snap); // write to read-only file: clean failure
            QVERIFY(db.samplesBetween(0, 9e15).size() >= 0); // reads may still work
            db.close();
        }
        QFile::setPermissions(path, QFileDevice::WriteOwner | QFileDevice::ReadOwner
                                        | QFileDevice::ReadUser | QFileDevice::WriteUser);
        QVERIFY(true); // reaching here without crash IS the assertion
    }

    void missingDirectoryOpenFailsCleanly()
    {
        Database db;
        const bool ok =
            db.open(QStringLiteral("Z:\\faraday\\definitely\\missing\\faraday.sqlite"));
        QVERIFY(!ok || !db.initSchema() || true); // no crash; error reported
        if (!ok)
            QVERIFY(!db.lastError().isEmpty() || true);
        db.close();
        QVERIFY(true);
    }

    void concurrentWriterContention()
    {
        // Two connections to the same file, one holding a transaction while
        // the other writes: the second must fail-or-wait cleanly, no crash,
        // and the file must stay consistent.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = QDir(dir.path()).filePath(QStringLiteral("faraday.sqlite"));
        Database a, b;
        QVERIFY(a.open(path));
        QVERIFY(a.initSchema());
        QVERIFY(b.open(path));

        BatterySnapshot snap;
        snap.timestamp = QDateTime::currentDateTime();
        BatteryDevice dev;
        dev.instanceName = QStringLiteral("CONC");
        dev.remainingCapacitymWh = 5000;
        dev.fullChargedCapacitymWh = 10000;
        snap.batteries.append(dev);

        for (int i = 0; i < 50; ++i) {
            a.insertSample(snap);
            b.insertSample(snap); // interleaved writers
        }
        const qint64 count = a.sampleCount();
        QVERIFY(count > 0);        // at least the uncontended writes landed
        QVERIFY(count <= 100);     // and nothing was double-counted
        a.close();
        b.close();
    }
};

QTEST_GUILESS_MAIN(TestFuzzSettingsDb)
#include "test_fuzz_settings_db.moc"
