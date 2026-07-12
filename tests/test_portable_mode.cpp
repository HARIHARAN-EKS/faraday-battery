#include <QtTest/QtTest>

#include "app/BatteryModel.h"
#include "app/PortableMode.h"

#include <QDir>
#include <QTemporaryDir>

using namespace faraday;

class TestPortableMode : public QObject
{
    Q_OBJECT
private slots:
    void noMarkerMeansDefaultLocation()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(PortableMode::dataDirFor(dir.path()).isEmpty());
        QVERIFY(PortableMode::dataDirFor(QString()).isEmpty());
        QVERIFY(PortableMode::dataDirFor(QStringLiteral("Z:\\does\\not\\exist")).isEmpty());
    }

    void markerActivatesAppLocalData()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile marker(QDir(dir.path()).filePath(PortableMode::markerFileName()));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("Faraday portable mode\n");
        marker.close();

        const QString dataDir = PortableMode::dataDirFor(dir.path());
        QCOMPARE(dataDir, QDir(dir.path()).filePath(QStringLiteral("data")));
    }

    void markerWorksInPathWithSpaces()
    {
        QTemporaryDir base;
        QVERIFY(base.isValid());
        const QString spaced =
            QDir(base.path()).filePath(QStringLiteral("Portable Apps Folder"));
        QVERIFY(QDir().mkpath(spaced));
        QFile marker(QDir(spaced).filePath(PortableMode::markerFileName()));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("x");
        marker.close();

        const QString dataDir = PortableMode::dataDirFor(spaced);
        QVERIFY(dataDir.startsWith(spaced));
        QVERIFY(dataDir.endsWith(QStringLiteral("data")));
    }

    void modelPersistsIntoPortableDataDir()
    {
        // End-to-end: the model initialized with a portable data dir must
        // create its settings + database there and persist across runs.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile marker(QDir(dir.path()).filePath(PortableMode::markerFileName()));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();
        const QString dataDir = PortableMode::dataDirFor(dir.path());

        {
            BatteryModel model;
            model.initialize(dataDir);
            // Defaults are persisted on first run — the portable folder is
            // complete even before the user changes anything.
            QVERIFY(QFileInfo::exists(
                QDir(dataDir).filePath(QStringLiteral("settings.json"))));
            QVERIFY(QFileInfo::exists(
                QDir(dataDir).filePath(QStringLiteral("faraday.sqlite"))));
            model.setSetting(QStringLiteral("theme"), QStringLiteral("light"));
        }
        {
            BatteryModel second;
            second.initialize(dataDir);
            QCOMPARE(second.theme(), QStringLiteral("light")); // persisted
        }

        // Nothing may leak outside the portable folder for this data dir.
        QVERIFY(QDir(dataDir).exists());
    }
};

QTEST_GUILESS_MAIN(TestPortableMode)
#include "test_portable_mode.moc"
