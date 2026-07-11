#include "Autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>

namespace faraday {

namespace {

QString defaultStartupDir()
{
    // %APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup
    PWSTR raw = nullptr;
    QString result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &raw)) && raw)
        result = QString::fromWCharArray(raw);
    if (raw)
        CoTaskMemFree(raw);
    return result;
}

bool createShortcut(const QString &linkPath, const QString &targetPath, QString *errorOut)
{
    const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool needUninit = (hrInit == S_OK || hrInit == S_FALSE);

    bool ok = false;
    IShellLinkW *link = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, reinterpret_cast<void **>(&link));
    if (SUCCEEDED(hr) && link) {
        const QString native = QDir::toNativeSeparators(targetPath);
        link->SetPath(reinterpret_cast<const wchar_t *>(native.utf16()));
        const QString workDir = QDir::toNativeSeparators(
            QFileInfo(targetPath).absolutePath());
        link->SetWorkingDirectory(reinterpret_cast<const wchar_t *>(workDir.utf16()));
        link->SetDescription(L"Faraday - Battery Intelligence Suite");

        IPersistFile *file = nullptr;
        hr = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file));
        if (SUCCEEDED(hr) && file) {
            const QString nativeLink = QDir::toNativeSeparators(linkPath);
            hr = file->Save(reinterpret_cast<const wchar_t *>(nativeLink.utf16()), TRUE);
            ok = SUCCEEDED(hr);
            if (!ok && errorOut)
                *errorOut = QStringLiteral("IPersistFile::Save failed (0x%1)")
                                .arg(static_cast<quint32>(hr), 8, 16, QLatin1Char('0'));
            file->Release();
        } else if (errorOut) {
            *errorOut = QStringLiteral("IPersistFile unavailable");
        }
        link->Release();
    } else if (errorOut) {
        *errorOut = QStringLiteral("CoCreateInstance(ShellLink) failed");
    }

    if (needUninit)
        CoUninitialize();
    return ok;
}

} // namespace

QString Autostart::shortcutPath(const QString &startupDir)
{
    QString dir = startupDir;
    if (dir.isEmpty())
        dir = defaultStartupDir();
    if (dir.isEmpty())
        return QString();
    return QDir(dir).filePath(QStringLiteral("Faraday.lnk"));
}

bool Autostart::isEnabled(const QString &startupDir)
{
    const QString path = shortcutPath(startupDir);
    return !path.isEmpty() && QFile::exists(path);
}

bool Autostart::setEnabled(bool enabled, const QString &startupDir,
                           const QString &targetOverride, QString *errorOut)
{
    const QString path = shortcutPath(startupDir);
    if (path.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Startup folder could not be resolved");
        return false;
    }

    if (!enabled) {
        if (!QFile::exists(path))
            return true;
        if (QFile::remove(path))
            return true;
        if (errorOut)
            *errorOut = QStringLiteral("Could not remove %1").arg(path);
        return false;
    }

    QString target = targetOverride;
    if (target.isEmpty())
        target = QCoreApplication::applicationFilePath();
    if (target.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("No executable path available");
        return false;
    }
    return createShortcut(path, target, errorOut);
}

} // namespace faraday
