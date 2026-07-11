#pragma once

#include "acquisition/BatteryReader.h"

#include <QObject>
#include <QTimer>

#include <memory>

namespace faraday {

// Periodic battery sampler designed to live on a worker thread
// (moveToThread before start()). The BatteryReader is created inside
// start() so its COM apartment belongs to the worker thread; the UI
// thread only ever receives snapshots through the queued signal.
class Sampler : public QObject
{
    Q_OBJECT
public:
    explicit Sampler(QObject *parent = nullptr);
    ~Sampler() override;

    bool isRunning() const { return m_timer && m_timer->isActive(); }
    int intervalSec() const { return m_intervalSec; }

public slots:
    void start(int intervalSec);
    void stop();
    void setIntervalSec(int intervalSec);
    void sampleNow();

signals:
    void snapshotReady(const faraday::BatterySnapshot &snapshot);
    void acquisitionError(const QString &message);

private:
    std::unique_ptr<BatteryReader> m_reader;
    QTimer *m_timer = nullptr;
    int m_intervalSec = 30;
    bool m_initialized = false;
};

} // namespace faraday
