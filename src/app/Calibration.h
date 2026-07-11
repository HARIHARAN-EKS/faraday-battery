#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

namespace faraday {

// Guided battery-conditioning (calibration) workflow.
//
// Sequence: charge to full -> rest at full -> discharge to low on battery
// -> recharge to full. This re-synchronizes the fuel gauge with the pack's
// real capacity, which fixes calibration drift.
class Calibration : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int step READ step NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString stepName READ stepName NOTIFY changed)
    Q_PROPERTY(QString instruction READ instruction NOTIFY changed)

public:
    enum Step {
        Idle = 0,
        ChargeToFull,
        RestAtFull,
        DischargeToLow,
        RechargeToFull,
        Done,
    };
    Q_ENUM(Step)

    static const int kRestSeconds = 30 * 60;
    static constexpr double kFullPercent = 98.0;
    static constexpr double kLowPercent = 10.0;
    static constexpr double kDonePercent = 99.0;

    explicit Calibration(QObject *parent = nullptr);

    int step() const { return m_step; }
    bool active() const { return m_step != Idle && m_step != Done; }
    QString stepName() const { return stepNameFor(static_cast<Step>(m_step)); }
    QString instruction() const { return instructionFor(static_cast<Step>(m_step)); }

    Q_INVOKABLE void start();
    Q_INVOKABLE void cancel();

    // Feed live conditions; `now` is injectable for tests.
    void update(double percent, bool onAcPower,
                const QDateTime &now = QDateTime::currentDateTime());

    // Pure transition function (unit-testable).
    static Step computeNext(Step current, double percent, bool onAcPower,
                            qint64 secondsInStep);

    static QString stepNameFor(Step step);
    static QString instructionFor(Step step);

    // Drift above this magnitude (in percent points) suggests conditioning.
    static bool driftIndicatesCalibration(double driftPercent) {
        return driftPercent > 5.0 || driftPercent < -5.0;
    }

signals:
    void changed();
    void finished();

private:
    void setStep(Step step, const QDateTime &now);

    int m_step = Idle;
    QDateTime m_stepEnteredAt;
};

} // namespace faraday
