#include <QtTest/QtTest>

#include "app/Calibration.h"

#include <QSignalSpy>

using namespace faraday;

class TestCalibration : public QObject
{
    Q_OBJECT
private slots:
    void transitionsChargeToFull()
    {
        QCOMPARE(Calibration::computeNext(Calibration::ChargeToFull, 50, true, 0),
                 Calibration::ChargeToFull);
        QCOMPARE(Calibration::computeNext(Calibration::ChargeToFull, 98, false, 0),
                 Calibration::ChargeToFull); // full but unplugged: keep waiting
        QCOMPARE(Calibration::computeNext(Calibration::ChargeToFull, 98.5, true, 0),
                 Calibration::RestAtFull);
    }

    void transitionsRestAtFull()
    {
        QCOMPARE(Calibration::computeNext(Calibration::RestAtFull, 100, true, 60),
                 Calibration::RestAtFull);
        QCOMPARE(Calibration::computeNext(Calibration::RestAtFull, 100, true, 1800),
                 Calibration::DischargeToLow);
        // User unplugged early: move on rather than stall.
        QCOMPARE(Calibration::computeNext(Calibration::RestAtFull, 99, false, 60),
                 Calibration::DischargeToLow);
    }

    void transitionsDischargeToLow()
    {
        QCOMPARE(Calibration::computeNext(Calibration::DischargeToLow, 50, false, 0),
                 Calibration::DischargeToLow);
        QCOMPARE(Calibration::computeNext(Calibration::DischargeToLow, 50, true, 0),
                 Calibration::DischargeToLow); // plugged in: instruction says unplug
        QCOMPARE(Calibration::computeNext(Calibration::DischargeToLow, 9.5, false, 0),
                 Calibration::RechargeToFull);
    }

    void transitionsRechargeToFull()
    {
        QCOMPARE(Calibration::computeNext(Calibration::RechargeToFull, 60, true, 0),
                 Calibration::RechargeToFull);
        QCOMPARE(Calibration::computeNext(Calibration::RechargeToFull, 99.5, false, 0),
                 Calibration::RechargeToFull); // needs AC
        QCOMPARE(Calibration::computeNext(Calibration::RechargeToFull, 99.5, true, 0),
                 Calibration::Done);
    }

    void idleAndDoneAreTerminal()
    {
        QCOMPARE(Calibration::computeNext(Calibration::Idle, 100, true, 9999),
                 Calibration::Idle);
        QCOMPARE(Calibration::computeNext(Calibration::Done, 5, false, 9999),
                 Calibration::Done);
    }

    void unknownPercentHoldsPosition()
    {
        QCOMPARE(Calibration::computeNext(Calibration::DischargeToLow, -1, false, 0),
                 Calibration::DischargeToLow);
    }

    void fullGuidedWalkthrough()
    {
        Calibration cal;
        QSignalSpy changedSpy(&cal, &Calibration::changed);
        QSignalSpy finishedSpy(&cal, &Calibration::finished);

        QCOMPARE(cal.step(), int(Calibration::Idle));
        QVERIFY(!cal.active());

        QDateTime now(QDate(2026, 7, 12), QTime(9, 0));
        cal.start();
        QVERIFY(cal.active());
        QCOMPARE(cal.step(), int(Calibration::ChargeToFull));

        cal.update(80, true, now);
        QCOMPARE(cal.step(), int(Calibration::ChargeToFull));
        now = now.addSecs(1200);
        cal.update(99, true, now);
        QCOMPARE(cal.step(), int(Calibration::RestAtFull));

        now = now.addSecs(600); // 10 min: still resting
        cal.update(100, true, now);
        QCOMPARE(cal.step(), int(Calibration::RestAtFull));
        now = now.addSecs(1300); // 31+ min total
        cal.update(100, true, now);
        QCOMPARE(cal.step(), int(Calibration::DischargeToLow));

        now = now.addSecs(3600);
        cal.update(40, false, now);
        QCOMPARE(cal.step(), int(Calibration::DischargeToLow));
        now = now.addSecs(3600);
        cal.update(9, false, now);
        QCOMPARE(cal.step(), int(Calibration::RechargeToFull));

        now = now.addSecs(3600);
        cal.update(99.5, true, now);
        QCOMPARE(cal.step(), int(Calibration::Done));
        QVERIFY(!cal.active());
        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY(changedSpy.count() >= 5);

        // Step names and instructions exist for every step.
        for (int s = Calibration::Idle; s <= Calibration::Done; ++s) {
            QVERIFY(!Calibration::stepNameFor(Calibration::Step(s)).isEmpty());
            QVERIFY(!Calibration::instructionFor(Calibration::Step(s)).isEmpty());
        }
    }

    void cancelReturnsToIdle()
    {
        Calibration cal;
        cal.start();
        QVERIFY(cal.active());
        cal.cancel();
        QCOMPARE(cal.step(), int(Calibration::Idle));
        // update() while idle is a no-op
        cal.update(99, true);
        QCOMPARE(cal.step(), int(Calibration::Idle));
    }

    void driftHeuristic()
    {
        QVERIFY(!Calibration::driftIndicatesCalibration(0.0));
        QVERIFY(!Calibration::driftIndicatesCalibration(4.9));
        QVERIFY(!Calibration::driftIndicatesCalibration(-4.9));
        QVERIFY(Calibration::driftIndicatesCalibration(5.1));
        QVERIFY(Calibration::driftIndicatesCalibration(-5.1));
    }
};

QTEST_APPLESS_MAIN(TestCalibration)
#include "test_calibration.moc"
