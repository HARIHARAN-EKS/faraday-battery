#include <QtTest/QtTest>

#include "data/Settings.h"

#include <QDir>
#include <QFile>

using namespace faraday;

class TestSettings : public QObject
{
    Q_OBJECT

    QString m_dir;

private slots:
    void initTestCase()
    {
        m_dir = QDir::temp().filePath(QStringLiteral("faraday_test_settings"));
        QDir(m_dir).removeRecursively();
        QDir().mkpath(m_dir);
    }

    void defaultsWhenFileMissing()
    {
        Settings s(m_dir);
        QVERIFY(!s.load()); // no file yet -> defaults
        QCOMPARE(s.sampleIntervalSec(), 30);
        QCOMPARE(s.theme(), QStringLiteral("dark"));
        QCOMPARE(s.temperatureUnit(), QStringLiteral("C"));
        QCOMPARE(s.lowLevelThresholdPct(), 20);
        QVERIFY(!s.launchWithWindows()); // must be opt-in
        QVERIFY(s.alertsEnabled());
    }

    void saveAndLoadRoundTrip()
    {
        {
            Settings s(m_dir);
            s.setValue(QStringLiteral("sampleIntervalSec"), 10);
            s.setValue(QStringLiteral("theme"), QStringLiteral("light"));
            s.setValue(QStringLiteral("temperatureUnit"), QStringLiteral("F"));
            s.setValue(QStringLiteral("lowLevelThresholdPct"), 35);
            QVERIFY2(s.save(), qPrintable(s.lastError()));
            QVERIFY(QFile::exists(s.filePath()));
        }
        {
            Settings s(m_dir);
            QVERIFY(s.load());
            QCOMPARE(s.sampleIntervalSec(), 10);
            QCOMPARE(s.theme(), QStringLiteral("light"));
            QCOMPARE(s.temperatureUnit(), QStringLiteral("F"));
            QCOMPARE(s.lowLevelThresholdPct(), 35);
            // Untouched keys keep defaults
            QCOMPARE(s.energyUnit(), QStringLiteral("mWh"));
        }
    }

    void corruptFileFallsBackToDefaults()
    {
        const QString dir = m_dir + QStringLiteral("/corrupt");
        QDir().mkpath(dir);
        {
            QFile f(QDir(dir).filePath(QStringLiteral("settings.json")));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("{not json at all");
        }
        Settings s(dir);
        QVERIFY(!s.load());
        QVERIFY(!s.lastError().isEmpty());
        QCOMPARE(s.sampleIntervalSec(), 30); // defaults intact
    }

    void invalidValuesAreClamped()
    {
        Settings s(m_dir);
        s.setValue(QStringLiteral("sampleIntervalSec"), -5);
        QCOMPARE(s.sampleIntervalSec(), 30);
        s.setValue(QStringLiteral("sampleIntervalSec"), 999999);
        QCOMPARE(s.sampleIntervalSec(), 30);
        s.setValue(QStringLiteral("theme"), QStringLiteral("hotdog"));
        QCOMPARE(s.theme(), QStringLiteral("dark"));
        s.setValue(QStringLiteral("lowLevelThresholdPct"), 0);
        QCOMPARE(s.lowLevelThresholdPct(), 20);
        s.setValue(QStringLiteral("highTempThresholdC"), 500.0);
        QCOMPARE(s.highTempThresholdC(), 50.0);
        s.setValue(QStringLiteral("unplugReminderPct"), 10);
        QCOMPARE(s.unplugReminderPct(), 95);
    }

    void cleanupTestCase()
    {
        QDir(m_dir).removeRecursively();
    }
};

QTEST_APPLESS_MAIN(TestSettings)
#include "test_settings.moc"
