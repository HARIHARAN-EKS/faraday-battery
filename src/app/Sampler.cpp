#include "Sampler.h"

namespace faraday {

Sampler::Sampler(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<BatterySnapshot>();
}

Sampler::~Sampler() = default;

void Sampler::start(int intervalSec)
{
    setIntervalSec(intervalSec);

    if (!m_reader) {
        m_reader.reset(new BatteryReader());
        m_initialized = m_reader->initialize();
        if (!m_initialized)
            emit acquisitionError(QStringLiteral("No WMI namespace could be reached; "
                                                 "battery data is unavailable."));
    }

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::VeryCoarseTimer); // battery-friendly
        connect(m_timer, &QTimer::timeout, this, &Sampler::sampleNow);
    }
    m_timer->setInterval(m_intervalSec * 1000);
    m_timer->start();

    sampleNow();
}

void Sampler::stop()
{
    if (m_timer)
        m_timer->stop();
}

void Sampler::setIntervalSec(int intervalSec)
{
    m_intervalSec = qBound(1, intervalSec, 3600);
    if (m_timer && m_timer->isActive())
        m_timer->setInterval(m_intervalSec * 1000);
}

void Sampler::sampleNow()
{
    if (!m_reader || !m_initialized)
        return;
    emit snapshotReady(m_reader->read());
}

} // namespace faraday
