#include <QtTest/QtTest>

#include "app/Sampler.h"

#include <QSignalSpy>
#include <QThread>

using namespace faraday;

class TestSampler : public QObject
{
    Q_OBJECT
private slots:
    void clampsInterval()
    {
        Sampler sampler;
        sampler.setIntervalSec(-10);
        QCOMPARE(sampler.intervalSec(), 1);
        sampler.setIntervalSec(999999);
        QCOMPARE(sampler.intervalSec(), 3600);
        sampler.setIntervalSec(30);
        QCOMPARE(sampler.intervalSec(), 30);
    }

    void emitsSnapshotsFromWorkerThread()
    {
        QThread worker;
        Sampler *sampler = new Sampler();
        sampler->moveToThread(&worker);
        connect(&worker, &QThread::finished, sampler, &QObject::deleteLater);

        QSignalSpy snapshotSpy(sampler, &Sampler::snapshotReady);
        worker.start();

        // start() must run on the worker thread (COM apartment lives there).
        QMetaObject::invokeMethod(sampler, "start", Qt::QueuedConnection, Q_ARG(int, 3600));

        QVERIFY(snapshotSpy.wait(15000)); // immediate first sample
        QVERIFY(snapshotSpy.count() >= 1);
        const auto snapshot = snapshotSpy.first().first().value<BatterySnapshot>();
        QVERIFY(snapshot.timestamp.isValid());
        // On a laptop batteries exist; on a desktop/VM the snapshot is
        // still structurally valid with batteryPresent == false.
        if (snapshot.batteryPresent)
            QVERIFY(!snapshot.batteries.isEmpty());

        QMetaObject::invokeMethod(sampler, "stop", Qt::BlockingQueuedConnection);
        worker.quit();
        QVERIFY(worker.wait(10000));
    }

    void sampleNowBeforeStartIsSafe()
    {
        Sampler sampler;
        QSignalSpy spy(&sampler, &Sampler::snapshotReady);
        sampler.sampleNow(); // no reader yet: must be a silent no-op
        QCOMPARE(spy.count(), 0);
        sampler.stop(); // stop before start is also safe
    }
};

QTEST_GUILESS_MAIN(TestSampler)
#include "test_sampler.moc"
