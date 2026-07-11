#pragma once

#include <QString>

namespace faraday {

// Opt-in launch-with-Windows via a shortcut in the user's Startup folder.
// Deliberately NOT the Run registry key: Faraday never writes the registry.
class Autostart
{
public:
    // Default folder: %APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup.
    // The override exists for tests.
    static QString shortcutPath(const QString &startupDir = QString());
    static bool isEnabled(const QString &startupDir = QString());
    // Creates or removes the shortcut. targetOverride is for tests; the
    // real target is the running executable.
    static bool setEnabled(bool enabled, const QString &startupDir = QString(),
                           const QString &targetOverride = QString(),
                           QString *errorOut = nullptr);
};

} // namespace faraday
