#include <QtTest/QtTest>

#include "app/BatteryModel.h"
#include "app/PortableMode.h"

#include <QDir>
#include <QTemporaryDir>

using namespace faraday;

// Portable mode under the P2 layout: the marker lives in the app directory
// (beside faraday-core.exe) and user data lives in the TOP-LEVEL data
// folder — deliberately outside app\.
class TestPortableMode : public QObject
{
    Q_OBJECT

    // Builds  <root>/app  and returns it; optionally drops the marker.
    static QString makeAppDir(const QString &root, bool withMarker)
    {
        const QString appDir = QDir(root).filePath(QStringLiteral("app"));
        if (!QDir().mkpath(appDir))
            return QString();
        if (withMarker) {
            QFile marker(QDir(appDir).filePath(PortableMode::markerFileName()));
            if (!marker.open(QIODevice::WriteOnly))
                return QString();
            marker.write("Faraday portable mode\n");
        }
        return appDir;
    }

private slots:
    void noMarkerMeansDefaultLocation()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString appDir = makeAppDir(root.path(), false);
        QVERIFY(!appDir.isEmpty());
        QVERIFY(PortableMode::dataDirFor(appDir).isEmpty());
        QVERIFY(PortableMode::dataDirFor(QString()).isEmpty());
        QVERIFY(PortableMode::dataDirFor(QStringLiteral("Z:\\nope\\app")).isEmpty());
    }

    void markerPutsDataAtProgramRootNotInsideApp()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString appDir = makeAppDir(root.path(), true);
        QVERIFY(!appDir.isEmpty());

        const QString dataDir = PortableMode::dataDirFor(appDir);
        const QString expected = QDir(root.path()).filePath(QStringLiteral("data"));
        QCOMPARE(QDir(dataDir).absolutePath(), QDir(expected).absolutePath());
        // The critical contract: user data is NOT inside app\.
        QVERIFY(!QDir(dataDir).absolutePath()
                     .startsWith(QDir(appDir).absolutePath() + QLatin1Char('/')));
    }

    void markerWorksInPathWithSpaces()
    {
        QTemporaryDir base;
        QVERIFY(base.isValid());
        const QString root =
            QDir(base.path()).filePath(QStringLiteral("Portable Apps Folder/Faraday"));
        QVERIFY(QDir().mkpath(root));
        const QString appDir = makeAppDir(root, true);
        QVERIFY(!appDir.isEmpty());

        const QString dataDir = PortableMode::dataDirFor(appDir);
        QVERIFY(dataDir.startsWith(root));
        QVERIFY(dataDir.endsWith(QStringLiteral("data")));
    }

    void modelPersistsIntoPortableDataDir()
    {
        // End-to-end: the model initialized with the resolved portable data
        // dir creates its settings + database there and persists across runs.
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString appDir = makeAppDir(root.path(), true);
        QVERIFY(!appDir.isEmpty());
        const QString dataDir = PortableMode::dataDirFor(appDir);
        QVERIFY(!dataDir.isEmpty());

        {
            BatteryModel model;
            model.initialize(dataDir);
            // Defaults persist on first run: the folder is complete at once.
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
        // Nothing leaked into the runtime folder.
        QVERIFY(!QFileInfo::exists(QDir(appDir).filePath(QStringLiteral("settings.json"))));
        QVERIFY(!QFileInfo::exists(QDir(appDir).filePath(QStringLiteral("data"))));
    }
};

QTEST_GUILESS_MAIN(TestPortableMode)
#include "test_portable_mode.moc"
