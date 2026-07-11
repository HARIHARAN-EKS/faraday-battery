#include "Settings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace faraday {

QVariantMap Settings::defaults()
{
    QVariantMap map;
    map.insert(QStringLiteral("sampleIntervalSec"), 30);
    map.insert(QStringLiteral("theme"), QStringLiteral("dark"));
    map.insert(QStringLiteral("temperatureUnit"), QStringLiteral("C"));
    map.insert(QStringLiteral("energyUnit"), QStringLiteral("mWh"));
    map.insert(QStringLiteral("alertsEnabled"), true);
    map.insert(QStringLiteral("lowLevelThresholdPct"), 20);
    map.insert(QStringLiteral("highTempThresholdC"), 50.0);
    map.insert(QStringLiteral("lowVoltageThresholdmV"), 10500);
    map.insert(QStringLiteral("unplugAtFullReminder"), true);
    map.insert(QStringLiteral("unplugReminderPct"), 95);
    map.insert(QStringLiteral("chargeBelowReminder"), true);
    map.insert(QStringLiteral("chargeReminderPct"), 20);
    map.insert(QStringLiteral("minimizeToTray"), true);
    map.insert(QStringLiteral("launchWithWindows"), false); // strictly opt-in
    map.insert(QStringLiteral("language"), QStringLiteral("en"));
    return map;
}

Settings::Settings(const QString &directory)
{
    QString dir = directory;
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_filePath = QDir(dir).filePath(QStringLiteral("settings.json"));
    m_values = defaults();
}

bool Settings::load()
{
    m_values = defaults();

    QFile file(m_filePath);
    if (!file.exists()) {
        m_lastError = QStringLiteral("settings file does not exist yet");
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("cannot open %1").arg(m_filePath);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("corrupt settings file: %1").arg(parseError.errorString());
        return false; // defaults stay in effect
    }

    const QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        m_values.insert(it.key(), it.value().toVariant());
    m_lastError.clear();
    return true;
}

bool Settings::save()
{
    const QDir dir = QFileInfo(m_filePath).dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        m_lastError = QStringLiteral("cannot create %1").arg(dir.absolutePath());
        return false;
    }

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QStringLiteral("cannot write %1").arg(m_filePath);
        return false;
    }
    const QJsonObject obj = QJsonObject::fromVariantMap(m_values);
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        m_lastError = QStringLiteral("failed to commit %1").arg(m_filePath);
        return false;
    }
    m_lastError.clear();
    return true;
}

QVariant Settings::value(const QString &key) const
{
    return m_values.value(key, defaults().value(key));
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    m_values.insert(key, value);
}

int Settings::sampleIntervalSec() const
{
    const int v = value(QStringLiteral("sampleIntervalSec")).toInt();
    return v >= 1 && v <= 3600 ? v : 30;
}

QString Settings::theme() const
{
    const QString t = value(QStringLiteral("theme")).toString();
    return t == QLatin1String("light") ? t : QStringLiteral("dark");
}

QString Settings::temperatureUnit() const
{
    const QString u = value(QStringLiteral("temperatureUnit")).toString();
    return u == QLatin1String("F") ? u : QStringLiteral("C");
}

QString Settings::energyUnit() const
{
    const QString u = value(QStringLiteral("energyUnit")).toString();
    return u == QLatin1String("Wh") ? u : QStringLiteral("mWh");
}

bool Settings::alertsEnabled() const
{
    return value(QStringLiteral("alertsEnabled")).toBool();
}

int Settings::lowLevelThresholdPct() const
{
    const int v = value(QStringLiteral("lowLevelThresholdPct")).toInt();
    return v >= 1 && v <= 99 ? v : 20;
}

double Settings::highTempThresholdC() const
{
    const double v = value(QStringLiteral("highTempThresholdC")).toDouble();
    return v >= 30.0 && v <= 90.0 ? v : 50.0;
}

int Settings::lowVoltageThresholdmV() const
{
    const int v = value(QStringLiteral("lowVoltageThresholdmV")).toInt();
    return v >= 1000 && v <= 30000 ? v : 10500;
}

bool Settings::unplugAtFullReminder() const
{
    return value(QStringLiteral("unplugAtFullReminder")).toBool();
}

int Settings::unplugReminderPct() const
{
    const int v = value(QStringLiteral("unplugReminderPct")).toInt();
    return v >= 50 && v <= 100 ? v : 95;
}

bool Settings::chargeBelowReminder() const
{
    return value(QStringLiteral("chargeBelowReminder")).toBool();
}

int Settings::chargeReminderPct() const
{
    const int v = value(QStringLiteral("chargeReminderPct")).toInt();
    return v >= 5 && v <= 50 ? v : 20;
}

bool Settings::minimizeToTray() const
{
    return value(QStringLiteral("minimizeToTray")).toBool();
}

bool Settings::launchWithWindows() const
{
    return value(QStringLiteral("launchWithWindows")).toBool();
}

QString Settings::language() const
{
    const QString l = value(QStringLiteral("language")).toString();
    return l.isEmpty() ? QStringLiteral("en") : l;
}

} // namespace faraday
