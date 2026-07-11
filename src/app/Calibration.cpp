#include "Calibration.h"

namespace faraday {

Calibration::Calibration(QObject *parent)
    : QObject(parent)
{
}

void Calibration::start()
{
    setStep(ChargeToFull, QDateTime::currentDateTime());
}

void Calibration::cancel()
{
    setStep(Idle, QDateTime::currentDateTime());
}

void Calibration::setStep(Step step, const QDateTime &now)
{
    if (m_step == step)
        return;
    m_step = step;
    m_stepEnteredAt = now;
    emit changed();
    if (step == Done)
        emit finished();
}

Calibration::Step Calibration::computeNext(Step current, double percent, bool onAcPower,
                                           qint64 secondsInStep)
{
    if (percent < 0)
        return current; // unknown level: hold position

    switch (current) {
    case Idle:
    case Done:
        return current;
    case ChargeToFull:
        return (onAcPower && percent >= kFullPercent) ? RestAtFull : ChargeToFull;
    case RestAtFull:
        // Keep resting while plugged in; the timer only counts settled time.
        if (!onAcPower)
            return DischargeToLow; // user unplugged early: proceed
        return secondsInStep >= kRestSeconds ? DischargeToLow : RestAtFull;
    case DischargeToLow:
        if (onAcPower && percent > kLowPercent)
            return DischargeToLow; // still plugged: instruction says unplug
        return (!onAcPower && percent <= kLowPercent) ? RechargeToFull : DischargeToLow;
    case RechargeToFull:
        return (onAcPower && percent >= kDonePercent) ? Done : RechargeToFull;
    }
    return current;
}

void Calibration::update(double percent, bool onAcPower, const QDateTime &now)
{
    if (!active())
        return;
    const qint64 inStep = m_stepEnteredAt.isValid() ? m_stepEnteredAt.secsTo(now) : 0;
    const Step next = computeNext(static_cast<Step>(m_step), percent, onAcPower, inStep);
    if (next != m_step)
        setStep(next, now);
}

QString Calibration::stepNameFor(Step step)
{
    switch (step) {
    case Idle:           return tr("Not running");
    case ChargeToFull:   return tr("Step 1 — Charge to full");
    case RestAtFull:     return tr("Step 2 — Rest at full");
    case DischargeToLow: return tr("Step 3 — Discharge to 10%");
    case RechargeToFull: return tr("Step 4 — Recharge to full");
    case Done:           return tr("Complete");
    }
    return QString();
}

QString Calibration::instructionFor(Step step)
{
    switch (step) {
    case Idle:
        return tr("Start the guided conditioning cycle to re-synchronize the "
                  "fuel gauge. Plan for a few hours; you can keep using the machine.");
    case ChargeToFull:
        return tr("Plug in the charger and let the battery reach 100%.");
    case RestAtFull:
        return tr("Stay plugged in for about 30 minutes so the cells settle "
                  "at full charge.");
    case DischargeToLow:
        return tr("Unplug the charger and use the machine on battery until the "
                  "level drops to 10%.");
    case RechargeToFull:
        return tr("Plug the charger back in and charge to 100% without "
                  "interruption.");
    case Done:
        return tr("Conditioning finished. The gauge and the pack are back in "
                  "sync; drift should now be minimal.");
    }
    return QString();
}

} // namespace faraday
