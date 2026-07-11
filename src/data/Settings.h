#pragma once

#include <QString>
#include <QVariant>
#include <QVariantMap>

namespace faraday {

// User preferences, persisted as JSON in an app-local file
// (%APPDATA%\Faraday Project\Faraday\settings.json by default).
// Deliberately NOT QSettings: the native backend would write the registry,
// which Faraday never does.
class Settings
{
public:
    // Uses the default app-data location when directory is empty.
    explicit Settings(const QString &directory = QString());

    bool load();  // missing or corrupt file falls back to defaults (returns false)
    bool save();
    QString filePath() const { return m_filePath; }
    QString lastError() const { return m_lastError; }

    QVariant value(const QString &key) const;
    void setValue(const QString &key, const QVariant &value);

    // Typed accessors with defaults
    int sampleIntervalSec() const;
    QString theme() const;              // "dark" | "light"
    QString temperatureUnit() const;    // "C" | "F"
    QString energyUnit() const;         // "mWh" | "Wh"
    bool alertsEnabled() const;
    int lowLevelThresholdPct() const;
    double highTempThresholdC() const;
    int lowVoltageThresholdmV() const;
    bool unplugAtFullReminder() const;
    int unplugReminderPct() const;
    bool chargeBelowReminder() const;
    int chargeReminderPct() const;
    bool minimizeToTray() const;
    bool launchWithWindows() const;     // opt-in, off by default
    QString language() const;

    static QVariantMap defaults();

private:
    QString m_filePath;
    QVariantMap m_values;
    QString m_lastError;
};

} // namespace faraday
