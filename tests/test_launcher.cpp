#include <QtTest/QtTest>

#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>

// D3 negative tests for the launcher stub: no launch path may ever reach
// the raw Windows loader dialog. The launcher is driven with
// FARADAY_LAUNCHER_NO_UI=1 (stderr + exit code instead of a MessageBox).
// Exit contract: 0 = app started, 2 = runtime incomplete, 3 = app failed
// to start / died immediately.
//
// Two scenarios from the D3 list run as scripted verifications outside
// this suite (recorded in the round report) because they need shell COM /
// drive mapping: Explorer's built-in ZIP extractor, and a subst'd
// non-system/USB-style drive. The wrong-working-directory *shortcut* case
// is covered here by launching with a foreign working directory — a .lnk
// is exactly a CreateProcess with lpCurrentDirectory set.
class TestLauncher : public QObject
{
    Q_OBJECT

    QString m_dist;      // source dist\Faraday (skip everything if absent)
    QTemporaryDir m_base;
    QString m_stage;     // reusable full stage for reversible mutations

    static bool copyTree(const QString &from, const QString &to)
    {
        QDir().mkpath(to);
        QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString src = it.next();
            const QString rel = QDir(from).relativeFilePath(src);
            const QString dst = QDir(to).filePath(rel);
            QDir().mkpath(QFileInfo(dst).absolutePath());
            if (!QFile::copy(src, dst))
                return false;
        }
        return true;
    }

    struct RunResult
    {
        int exitCode = -1;
        QString stderrText;
        bool finished = false;
    };

    static RunResult runLauncher(const QString &dir, const QString &workingDir = QString())
    {
        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("FARADAY_LAUNCHER_NO_UI"), QStringLiteral("1"));
        // Field machines have no Qt on PATH; the developer machine does.
        // Strip it so the child resolves DLLs exactly as it would in the
        // field (exe directory first, then system dirs).
        const QStringList parts =
            env.value(QStringLiteral("PATH")).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        QStringList filtered;
        for (const QString &p : parts) {
            if (!p.contains(QStringLiteral("\\Qt\\"), Qt::CaseInsensitive))
                filtered << p;
        }
        env.insert(QStringLiteral("PATH"), filtered.join(QLatin1Char(';')));
        proc.setProcessEnvironment(env);
        proc.setWorkingDirectory(workingDir.isEmpty() ? dir : workingDir);
        proc.start(QDir(dir).filePath(QStringLiteral("faraday.exe")), {});
        RunResult r;
        r.finished = proc.waitForFinished(20000);
        if (!r.finished)
            proc.kill();
        r.exitCode = proc.exitCode();
        r.stderrText = QString::fromUtf8(proc.readAllStandardError());
        return r;
    }

    static void killApp()
    {
        QProcess::execute(QStringLiteral("taskkill"),
                          { QStringLiteral("/F"), QStringLiteral("/IM"),
                            QStringLiteral("faraday-app.exe") });
        QTest::qWait(500);
    }

    static bool appRunning()
    {
        QProcess p;
        p.start(QStringLiteral("tasklist"),
                { QStringLiteral("/FI"), QStringLiteral("IMAGENAME eq faraday-app.exe") });
        p.waitForFinished(10000);
        return QString::fromLocal8Bit(p.readAllStandardOutput())
            .contains(QStringLiteral("faraday-app.exe"));
    }

private slots:
    void initTestCase()
    {
        m_dist = QDir(QStringLiteral(QT_TESTCASE_SOURCEDIR))
                     .filePath(QStringLiteral("../dist/Faraday"));
        if (!QFileInfo::exists(QDir(m_dist).filePath(QStringLiteral("runtime.manifest"))))
            QSKIP("packaged dist not present; launcher suite needs dist\\Faraday");
        QVERIFY(m_base.isValid());
        m_stage = QDir(m_base.path()).filePath(QStringLiteral("stage/Faraday"));
        QVERIFY(copyTree(m_dist, m_stage));
        // Isolate: portable marker keeps app data inside the stage.
        QFile marker(QDir(m_stage).filePath(QStringLiteral("portable.txt")));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();
    }

    void cleanup() { killApp(); }

    void fullStageRunsCleanly()
    {
        const RunResult r = runLauncher(m_stage);
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 0);
        QVERIFY(appRunning());
    }

    void bareExeAloneShowsFriendlyMessage()
    {
        const QString bare = QDir(m_base.path()).filePath(QStringLiteral("bare"));
        QDir().mkpath(bare);
        QVERIFY(QFile::copy(QDir(m_stage).filePath(QStringLiteral("faraday.exe")),
                            QDir(bare).filePath(QStringLiteral("faraday.exe"))));
        const RunResult r = runLauncher(bare);
        QVERIFY(r.finished);                 // never hangs, never a loader box
        QCOMPARE(r.exitCode, 2);
        QVERIFY2(r.stderrText.contains(QStringLiteral("extract the ENTIRE"),
                                       Qt::CaseInsensitive),
                 qPrintable(r.stderrText));
    }

    void halfExtractedNoManifestStillFriendly()
    {
        // Launcher + app exe only (no manifest, no runtime): the pre-check
        // cannot see the manifest, so the spawn-watch must catch the child's
        // loader death — with the error dialog suppressed by inheritance.
        const QString half = QDir(m_base.path()).filePath(QStringLiteral("half"));
        QDir().mkpath(half);
        QVERIFY(QFile::copy(QDir(m_stage).filePath(QStringLiteral("faraday.exe")),
                            QDir(half).filePath(QStringLiteral("faraday.exe"))));
        QVERIFY(QFile::copy(QDir(m_stage).filePath(QStringLiteral("faraday-app.exe")),
                            QDir(half).filePath(QStringLiteral("faraday-app.exe"))));
        const RunResult r = runLauncher(half);
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 3);
        QVERIFY2(r.stderrText.contains(QStringLiteral("incomplete or damaged")),
                 qPrintable(r.stderrText));
    }

    void everyManifestEntryDeleted_data()
    {
        QTest::addColumn<QString>("entry");
        QFile manifest(QDir(m_dist).filePath(QStringLiteral("runtime.manifest")));
        if (!manifest.open(QIODevice::ReadOnly))
            return;
        const QStringList entries =
            QString::fromUtf8(manifest.readAll()).split(QRegularExpression(
                QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
        for (const QString &e : entries)
            QTest::addRow("%s", qPrintable(e)) << e;
    }
    void everyManifestEntryDeleted()
    {
        QFETCH(QString, entry);
        const QString victim = QDir(m_stage).filePath(entry);
        const QString hidden = victim + QStringLiteral(".hidden");
        QVERIFY2(QFile::rename(victim, hidden), qPrintable(entry));
        const RunResult r = runLauncher(m_stage);
        QVERIFY(QFile::rename(hidden, victim)); // restore before asserting
        QVERIFY(r.finished);
        QVERIFY2(r.exitCode == 2, qPrintable(QStringLiteral("%1: exit=%2 err=%3")
                                                 .arg(entry).arg(r.exitCode)
                                                 .arg(r.stderrText)));
        // Deleting faraday-app.exe itself lands on the bare-exe advice;
        // every other entry gets the precise missing-files message.
        QVERIFY(r.stderrText.contains(QStringLiteral("missing"))
                || r.stderrText.contains(QStringLiteral("ENTIRE")));
    }

    void missingPlatformsFolder()
    {
        const QString dir = QDir(m_stage).filePath(QStringLiteral("platforms"));
        QVERIFY(QDir().rename(dir, dir + QStringLiteral(".hidden")));
        const RunResult r = runLauncher(m_stage);
        QVERIFY(QDir().rename(dir + QStringLiteral(".hidden"), dir));
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 2);
        QVERIFY(r.stderrText.contains(QStringLiteral("qwindows.dll")));
    }

    void missingQmlModuleDirectory()
    {
        const QString dir = QDir(m_stage).filePath(QStringLiteral("qml/QtQuick"));
        QVERIFY(QDir().rename(dir, dir + QStringLiteral(".hidden")));
        const RunResult r = runLauncher(m_stage);
        QVERIFY(QDir().rename(dir + QStringLiteral(".hidden"), dir));
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 2);
        QVERIFY(r.stderrText.contains(QStringLiteral("missing")));
    }

    void corruptedDllYieldsFriendlyMessageNotLoaderBox()
    {
        // Fresh stage (destructive mutation).
        const QString corrupt = QDir(m_base.path()).filePath(QStringLiteral("corrupt/Faraday"));
        QVERIFY(copyTree(m_dist, corrupt));
        const QString dll = QDir(corrupt).filePath(QStringLiteral("Qt6Gui.dll"));
        {
            QFile f(dll);
            QVERIFY(f.open(QIODevice::ReadWrite));
            f.resize(f.size() / 2); // truncate: exists (manifest passes) but unloadable
        }
        const RunResult r = runLauncher(corrupt);
        QVERIFY(r.finished);                 // spawn-watch caught the child death
        QCOMPARE(r.exitCode, 3);
        QVERIFY2(r.stderrText.contains(QStringLiteral("incomplete or damaged")),
                 qPrintable(r.stderrText));
    }

    void foreignWorkingDirectoryStillRuns()
    {
        // Covers both a manual start from elsewhere and a shortcut with the
        // wrong "Start in" (identical CreateProcess semantics).
        QTemporaryDir elsewhere;
        QVERIFY(elsewhere.isValid());
        const RunResult r = runLauncher(m_stage, elsewhere.path());
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 0);
        QVERIFY(appRunning());
    }

    void pathWithSpacesRuns()
    {
        const QString spaced =
            QDir(m_base.path()).filePath(QStringLiteral("Portable Apps Here/Faraday"));
        QVERIFY(copyTree(m_dist, spaced));
        QFile marker(QDir(spaced).filePath(QStringLiteral("portable.txt")));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();
        const RunResult r = runLauncher(spaced);
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 0);
        QVERIFY(appRunning());
    }
};

QTEST_GUILESS_MAIN(TestLauncher)
#include "test_launcher.moc"
