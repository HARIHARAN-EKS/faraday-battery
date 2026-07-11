#include <QtTest/QtTest>

#include "app/Autostart.h"
#include "app/ChargeCap.h"

#include <QDir>
#include <QFile>

using namespace faraday;

class TestAutostart : public QObject
{
    Q_OBJECT

    QString m_dir;
    QString m_fakeTarget;

private slots:
    void initTestCase()
    {
        m_dir = QDir::temp().filePath(QStringLiteral("faraday_test_startup"));
        QDir(m_dir).removeRecursively();
        QDir().mkpath(m_dir);
        // A real file to point the shortcut at.
        m_fakeTarget = QDir(m_dir).filePath(QStringLiteral("target.exe"));
        QFile f(m_fakeTarget);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("stub");
    }

    void shortcutPathUsesOverride()
    {
        const QString path = Autostart::shortcutPath(m_dir);
        QVERIFY(path.endsWith(QStringLiteral("Faraday.lnk")));
        QVERIFY(path.startsWith(m_dir));
    }

    void enableCreatesShortcut()
    {
        QString error;
        QVERIFY(!Autostart::isEnabled(m_dir));
        const bool ok = Autostart::setEnabled(true, m_dir, m_fakeTarget, &error);
        QVERIFY2(ok, qPrintable(error));
        QVERIFY(Autostart::isEnabled(m_dir));
        QVERIFY(QFile::exists(Autostart::shortcutPath(m_dir)));
    }

    void disableRemovesShortcut()
    {
        QString error;
        QVERIFY(Autostart::setEnabled(false, m_dir, QString(), &error));
        QVERIFY(!Autostart::isEnabled(m_dir));
        // Disabling twice is a no-op success.
        QVERIFY(Autostart::setEnabled(false, m_dir, QString(), &error));
    }

    void defaultStartupFolderResolves()
    {
        // Must resolve on any Windows session (we do not create anything).
        const QString path = Autostart::shortcutPath();
        QVERIFY(!path.isEmpty());
        QVERIFY(path.contains(QStringLiteral("Startup"), Qt::CaseInsensitive));
    }

    void chargeCapProbeNeverCrashes()
    {
        // Vendor-specific: on most machines unsupported; must never crash
        // and setEnabled must fail cleanly when unsupported.
        const ChargeCapInfo info = ChargeCap::probe();
        if (!info.supported) {
            QString error;
            QVERIFY(!ChargeCap::setEnabled(true, &error));
            QVERIFY(!error.isEmpty());
        } else {
            QVERIFY(!info.vendor.isEmpty());
            QVERIFY(!info.settingName.isEmpty());
        }
    }

    void cleanupTestCase()
    {
        QDir(m_dir).removeRecursively();
    }
};

QTEST_GUILESS_MAIN(TestAutostart)
#include "test_autostart.moc"
