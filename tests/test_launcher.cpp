#include <QtTest/QtTest>

#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>

// Negative tests for the launcher stub, re-run against the P2 layout:
//
//   Faraday/
//     Faraday.exe        <- launched here
//     README.txt
//     app/               <- runtime + faraday-core.exe live here
//
// No launch path may ever reach the raw Windows loader dialog. The launcher
// is driven with FARADAY_LAUNCHER_NO_UI=1 (stderr + exit code instead of a
// MessageBox). Exit contract: 0 = app started, 2 = runtime incomplete,
// 3 = app failed to start / died immediately.
//
// Two scenarios from the D3/P4 list run as scripted verifications outside
// this suite (recorded in the round report) because they need shell COM /
// drive mapping: Explorer's built-in ZIP extractor, and a subst'd
// non-system (USB-style) drive. The wrong-working-directory *shortcut* case
// is covered here by launching with a foreign working directory — a .lnk is
// exactly a CreateProcess with lpCurrentDirectory set.
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

    // dir = the program root (the folder holding Faraday.exe).
    static RunResult runLauncher(const QString &dir, const QString &workingDir = QString())
    {
        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("FARADAY_LAUNCHER_NO_UI"), QStringLiteral("1"));
        // Field machines have no Qt on PATH; the developer machine does.
        // Strip it so the child resolves DLLs exactly as it would in the
        // field (its own directory first, then system dirs).
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
        proc.start(QDir(dir).filePath(QStringLiteral("Faraday.exe")), {});
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
                            QStringLiteral("faraday-core.exe") });
        QTest::qWait(500);
    }

    static bool appRunning()
    {
        QProcess p;
        p.start(QStringLiteral("tasklist"),
                { QStringLiteral("/FI"), QStringLiteral("IMAGENAME eq faraday-core.exe") });
        p.waitForFinished(10000);
        return QString::fromLocal8Bit(p.readAllStandardOutput())
            .contains(QStringLiteral("faraday-core.exe"));
    }

private slots:
    void initTestCase()
    {
        m_dist = QDir(QStringLiteral(QT_TESTCASE_SOURCEDIR))
                     .filePath(QStringLiteral("../dist/Faraday"));
        if (!QFileInfo::exists(
                QDir(m_dist).filePath(QStringLiteral("app/runtime.manifest")))) {
            QSKIP("packaged dist not present; launcher suite needs dist\\Faraday");
        }
        QVERIFY(m_base.isValid());
        m_stage = QDir(m_base.path()).filePath(QStringLiteral("stage/Faraday"));
        QVERIFY(copyTree(m_dist, m_stage));
        // Isolate: the portable marker (inside app\) keeps data in the
        // stage's own top-level data\ folder.
        QFile marker(QDir(m_stage).filePath(QStringLiteral("app/portable.txt")));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.close();
    }

    void cleanup() { killApp(); }

    // ---- layout presentation --------------------------------------------

    void distShowsExactlyTwoTopLevelItems()
    {
        // The whole point of P2: the user sees one thing to click.
        const QStringList top = QDir(m_dist).entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        QCOMPARE(top.size(), 3); // Faraday.exe, README.txt, app  (app is a dir)
        QVERIFY(top.contains(QStringLiteral("Faraday.exe")));
        QVERIFY(top.contains(QStringLiteral("README.txt")));
        QVERIFY(top.contains(QStringLiteral("app")));
        // Exactly two FILES at the top level; everything else is inside app\.
        const QStringList topFiles = QDir(m_dist).entryList(QDir::Files);
        QCOMPARE(topFiles.size(), 2);
        // And the runtime really is inside app\, not scattered.
        QVERIFY(QFileInfo::exists(QDir(m_dist).filePath(QStringLiteral("app/Qt6Core.dll"))));
        QVERIFY(!QFileInfo::exists(QDir(m_dist).filePath(QStringLiteral("Qt6Core.dll"))));
    }

    void coreExeIsSelfDocumenting()
    {
        // Anyone who opens app\ must be told what faraday-core.exe is.
        const QString core = QDir(m_dist).filePath(QStringLiteral("app/faraday-core.exe"));
        QVERIFY(QFileInfo::exists(core));
        QFile f(core);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray blob = f.readAll();
        // VersionInfo strings live as UTF-16LE in the PE resource section.
        const QString text = QStringLiteral("run Faraday.exe instead");
        const QByteArray needle(reinterpret_cast<const char *>(text.utf16()),
                                text.size() * 2);
        QVERIFY2(blob.contains(needle),
                 "faraday-core.exe must carry a self-documenting FileDescription");
    }

    // ---- launch paths ----------------------------------------------------

    void fullStageRunsCleanly()
    {
        const RunResult r = runLauncher(m_stage);
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 0);
        QVERIFY(appRunning());
    }

    void portableDataLandsAtProgramRootNotInsideApp()
    {
        const RunResult r = runLauncher(m_stage);
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 0);
        QTest::qWait(3000); // let the app write its first settings/db
        QVERIFY2(QFileInfo::exists(QDir(m_stage).filePath(QStringLiteral("data/settings.json"))),
                 "portable data must live in the TOP-LEVEL data folder");
        QVERIFY(!QFileInfo::exists(QDir(m_stage).filePath(QStringLiteral("app/data"))));
    }

    void bareExeAloneShowsFriendlyMessage()
    {
        // The original field failure: Faraday.exe copied out on its own.
        const QString bare = QDir(m_base.path()).filePath(QStringLiteral("bare"));
        QDir().mkpath(bare);
        QVERIFY(QFile::copy(QDir(m_stage).filePath(QStringLiteral("Faraday.exe")),
                            QDir(bare).filePath(QStringLiteral("Faraday.exe"))));
        const RunResult r = runLauncher(bare);
        QVERIFY(r.finished);                 // never hangs, never a loader box
        QCOMPARE(r.exitCode, 2);
        QVERIFY2(r.stderrText.contains(QStringLiteral("extract the ENTIRE"),
                                       Qt::CaseInsensitive),
                 qPrintable(r.stderrText));
    }

    void missingAppFolderShowsFriendlyMessage()
    {
        const QString noApp = QDir(m_base.path()).filePath(QStringLiteral("noapp"));
        QDir().mkpath(noApp);
        QVERIFY(QFile::copy(QDir(m_stage).filePath(QStringLiteral("Faraday.exe")),
                            QDir(noApp).filePath(QStringLiteral("Faraday.exe"))));
        QVERIFY(QFile::copy(QDir(m_stage).filePath(QStringLiteral("README.txt")),
                            QDir(noApp).filePath(QStringLiteral("README.txt"))));
        const RunResult r = runLauncher(noApp);
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 2);
        QVERIFY(r.stderrText.contains(QStringLiteral("'app' folder")));
    }

    void halfExtractedNoManifestStillFriendly()
    {
        // Launcher + core exe only (no manifest, no runtime): the pre-check
        // cannot see the manifest, so the spawn-watch must catch the child's
        // loader death — with the error dialog suppressed by inheritance.
        const QString half = QDir(m_base.path()).filePath(QStringLiteral("half"));
        QDir().mkpath(QDir(half).filePath(QStringLiteral("app")));
        QVERIFY(QFile::copy(QDir(m_stage).filePath(QStringLiteral("Faraday.exe")),
                            QDir(half).filePath(QStringLiteral("Faraday.exe"))));
        QVERIFY(QFile::copy(QDir(m_stage).filePath(QStringLiteral("app/faraday-core.exe")),
                            QDir(half).filePath(QStringLiteral("app/faraday-core.exe"))));
        const RunResult r = runLauncher(half);
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 3);
        QVERIFY2(r.stderrText.contains(QStringLiteral("incomplete or damaged")),
                 qPrintable(r.stderrText));
    }

    void everyManifestEntryDeleted_data()
    {
        QTest::addColumn<QString>("entry");
        QFile manifest(QDir(m_dist).filePath(QStringLiteral("app/runtime.manifest")));
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
        // Manifest paths are app-relative.
        const QString victim =
            QDir(m_stage).filePath(QStringLiteral("app/") + entry);
        const QString hidden = victim + QStringLiteral(".hidden");
        QVERIFY2(QFile::rename(victim, hidden), qPrintable(entry));
        const RunResult r = runLauncher(m_stage);
        QVERIFY(QFile::rename(hidden, victim)); // restore before asserting
        QVERIFY(r.finished);
        QVERIFY2(r.exitCode == 2, qPrintable(QStringLiteral("%1: exit=%2 err=%3")
                                                 .arg(entry).arg(r.exitCode)
                                                 .arg(r.stderrText)));
        // Deleting faraday-core.exe itself lands on the missing-app advice;
        // every other entry gets the precise missing-files message.
        QVERIFY(r.stderrText.contains(QStringLiteral("missing"))
                || r.stderrText.contains(QStringLiteral("'app' folder")));
    }

    void missingPlatformsFolder()
    {
        const QString dir = QDir(m_stage).filePath(QStringLiteral("app/platforms"));
        QVERIFY(QDir().rename(dir, dir + QStringLiteral(".hidden")));
        const RunResult r = runLauncher(m_stage);
        QVERIFY(QDir().rename(dir + QStringLiteral(".hidden"), dir));
        QVERIFY(r.finished);
        QCOMPARE(r.exitCode, 2);
        QVERIFY(r.stderrText.contains(QStringLiteral("qwindows.dll")));
    }

    void missingQmlModuleDirectory()
    {
        const QString dir = QDir(m_stage).filePath(QStringLiteral("app/qml/QtQuick"));
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
        const QString dll = QDir(corrupt).filePath(QStringLiteral("app/Qt6Gui.dll"));
        {
            QFile f(dll);
            QVERIFY(f.open(QIODevice::ReadWrite));
            f.resize(f.size() / 2); // exists (manifest passes) but unloadable
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
        // wrong "Start in" (identical CreateProcess semantics). The child's
        // CWD is forced to app\, so DLL resolution never depends on it.
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
        QFile marker(QDir(spaced).filePath(QStringLiteral("app/portable.txt")));
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
