#include <QtTest/QtTest>

#include "app/TrayManager.h"

#include <QApplication>

using namespace faraday;

// TrayManager needs a QApplication (widgets). On a normal desktop session
// the tray is available and every path runs; on a headless/CI session the
// unavailable path is what gets exercised — both are legitimate.
class TestTray : public QObject
{
    Q_OBJECT
private slots:
    void constructAndQueryAvailability()
    {
        TrayManager tray;
        // available() is environment-dependent; the contract is that it
        // answers without crashing and stays consistent across calls.
        const bool avail = tray.available();
        QCOMPARE(tray.available(), avail);
    }

    void tooltipAndAlertsNeverCrash()
    {
        TrayManager tray;
        tray.setTooltip(QStringLiteral("Faraday — 93% · Discharging"));
        tray.setTooltip(QString());                       // empty
        tray.setTooltip(QString(1024, QChar(u'x')));      // oversized
        if (tray.available()) {
            // One real notification exercises the balloon path end-to-end.
            tray.showAlert(QStringLiteral("Faraday self-test"),
                           QStringLiteral("Tray notification path check"));
        }
        tray.notifyMinimized();
        tray.notifyMinimized(); // second call: the notify-once guard branch
        QVERIFY(true);          // reaching here without crash is the assertion
    }

    void signalsAreWired()
    {
        TrayManager tray;
        QSignalSpy openSpy(&tray, &TrayManager::openRequested);
        QSignalSpy quitSpy(&tray, &TrayManager::quitRequested);
        QSignalSpy sampleSpy(&tray, &TrayManager::sampleNowRequested);
        QVERIFY(openSpy.isValid());
        QVERIFY(quitSpy.isValid());
        QVERIFY(sampleSpy.isValid());
    }
};

QTEST_MAIN(TestTray)
#include "test_tray.moc"
