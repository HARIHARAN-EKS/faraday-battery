#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <optional>

namespace faraday {

class Settings;

struct AlertInput
{
    double percent = -1;                 // 0..100, negative = unknown
    std::optional<double> voltageV;
    std::optional<double> temperatureC;
    bool charging = false;
    bool discharging = false;
    bool onAcPower = false;
};

struct Alert
{
    QString id;        // stable identifier ("lowLevel", "highTemp", ...)
    QString title;
    QString message;
};

// Threshold evaluation with latching: each alert fires once when its
// condition becomes true and re-arms only after the condition clears
// (with hysteresis where it makes sense), so the tray is never spammed.
class AlertManager : public QObject
{
    Q_OBJECT
public:
    explicit AlertManager(Settings *settings, QObject *parent = nullptr);

    // Pure evaluation; returns the alerts that fire on this transition.
    QList<Alert> evaluate(const AlertInput &input);

    void reset();

signals:
    void alertRaised(const QString &title, const QString &message);

public slots:
    // Convenience wrapper that also emits alertRaised for each alert.
    void process(const AlertInput &input);

private:
    Settings *m_settings;
    bool m_lowLevelLatched = false;
    bool m_highTempLatched = false;
    bool m_lowVoltageLatched = false;
    bool m_unplugLatched = false;
    bool m_chargeBelowLatched = false;
};

} // namespace faraday
