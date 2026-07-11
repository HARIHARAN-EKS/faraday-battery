#include "AlertManager.h"

#include "data/Settings.h"

namespace faraday {

AlertManager::AlertManager(Settings *settings, QObject *parent)
    : QObject(parent),
      m_settings(settings)
{
}

void AlertManager::reset()
{
    m_lowLevelLatched = false;
    m_highTempLatched = false;
    m_lowVoltageLatched = false;
    m_unplugLatched = false;
    m_chargeBelowLatched = false;
}

QList<Alert> AlertManager::evaluate(const AlertInput &input)
{
    QList<Alert> fired;
    if (!m_settings || !m_settings->alertsEnabled())
        return fired;

    // --- Low battery level (only meaningful while discharging) ------------
    const int lowPct = m_settings->lowLevelThresholdPct();
    if (input.percent >= 0 && !input.onAcPower && input.percent <= lowPct) {
        if (!m_lowLevelLatched) {
            m_lowLevelLatched = true;
            fired.append({ QStringLiteral("lowLevel"),
                           tr("Battery low"),
                           tr("Charge level is down to %1%. Consider plugging in.")
                               .arg(QString::number(input.percent, 'f', 0)) });
        }
    } else if (input.percent > lowPct + 5 || input.onAcPower) {
        m_lowLevelLatched = false;
    }

    // --- High temperature ---------------------------------------------------
    const double highTemp = m_settings->highTempThresholdC();
    if (input.temperatureC.has_value() && *input.temperatureC >= highTemp) {
        if (!m_highTempLatched) {
            m_highTempLatched = true;
            fired.append({ QStringLiteral("highTemp"),
                           tr("High temperature"),
                           tr("System temperature reached %1°C. Heat accelerates battery wear.")
                               .arg(QString::number(*input.temperatureC, 'f', 0)) });
        }
    } else if (!input.temperatureC.has_value() || *input.temperatureC < highTemp - 3) {
        m_highTempLatched = false;
    }

    // --- Low pack voltage -----------------------------------------------------
    const double lowVoltage = m_settings->lowVoltageThresholdmV() / 1000.0;
    if (input.voltageV.has_value() && *input.voltageV > 0 && *input.voltageV <= lowVoltage) {
        if (!m_lowVoltageLatched) {
            m_lowVoltageLatched = true;
            fired.append({ QStringLiteral("lowVoltage"),
                           tr("Low pack voltage"),
                           tr("Pack voltage dropped to %1 V (threshold %2 V).")
                               .arg(QString::number(*input.voltageV, 'f', 2),
                                    QString::number(lowVoltage, 'f', 2)) });
        }
    } else if (!input.voltageV.has_value() || *input.voltageV > lowVoltage + 0.3) {
        m_lowVoltageLatched = false;
    }

    // --- Unplug at full -------------------------------------------------------
    if (m_settings->unplugAtFullReminder()) {
        const int unplugPct = m_settings->unplugReminderPct();
        if (input.onAcPower && input.percent >= unplugPct) {
            if (!m_unplugLatched) {
                m_unplugLatched = true;
                fired.append({ QStringLiteral("unplugAtFull"),
                               tr("Battery charged"),
                               tr("The battery is at %1%. Unplugging now reduces "
                                  "time spent at full charge.")
                                   .arg(QString::number(input.percent, 'f', 0)) });
            }
        } else if (!input.onAcPower || input.percent < unplugPct - 3) {
            m_unplugLatched = false;
        }
    }

    // --- Charge below reminder --------------------------------------------------
    if (m_settings->chargeBelowReminder()) {
        const int chargePct = m_settings->chargeReminderPct();
        if (!input.onAcPower && input.percent >= 0 && input.percent <= chargePct) {
            if (!m_chargeBelowLatched) {
                m_chargeBelowLatched = true;
                fired.append({ QStringLiteral("chargeBelow"),
                               tr("Time to charge"),
                               tr("Charge level fell to %1%. Lithium batteries last "
                                  "longer when they are not run down deeply.")
                                   .arg(QString::number(input.percent, 'f', 0)) });
            }
        } else if (input.onAcPower || input.percent > chargePct + 5) {
            m_chargeBelowLatched = false;
        }
    }

    return fired;
}

void AlertManager::process(const AlertInput &input)
{
    const QList<Alert> alerts = evaluate(input);
    for (const Alert &alert : alerts)
        emit alertRaised(alert.title, alert.message);
}

} // namespace faraday
