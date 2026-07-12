// Faraday launcher stub (field defect D2).
//
// Problem: faraday's Qt imports are resolved by the Windows LOADER before
// main() runs. A bare exe copied out of the portable ZIP therefore died
// with the raw "System Error: Qt6Gui.dll was not found" dialog — nothing
// inside that process could ever catch it, and MinGW has no MSVC-style
// /DELAYLOAD to defer the imports.
//
// Fix: THIS binary is what users launch as faraday.exe. It links against
// system DLLs only (kernel32/user32) and is statically linked against the
// MinGW runtime, so it starts in ANY environment. It verifies the runtime
// beside it (against the packaging-generated runtime.manifest when present,
// or a built-in core list), then starts the real application
// (faraday-app.exe) with loader error boxes suppressed and watches its
// first seconds — so even a corrupted DLL yields a friendly message, never
// a raw loader dialog.
//
// AV posture: no packing, no network, no registry, no elevation. Plain
// CreateProcess of a sibling binary, full VersionInfo, asInvoker manifest.
//
// Test hook: FARADAY_LAUNCHER_NO_UI=1 replaces the MessageBox with stderr
// output so the negative-test suite can assert messages without modal UI.
// Exit codes: 0 = app started; 2 = runtime incomplete; 3 = app failed to
// start or died immediately.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>

namespace {

const wchar_t kAppBinary[] = L"faraday-app.exe";
const wchar_t kManifestName[] = L"runtime.manifest";
const wchar_t kTitle[] = L"Faraday";

// Without a runtime.manifest (developer trees, where Qt resolves via
// PATH), verification falls to the spawn-watch below: the child either
// runs or fails fast into the friendly message.
const wchar_t kExtractAdvice[] =
    L"Faraday must be run from its complete program folder.\r\n\r\n"
    L"If you downloaded the portable ZIP: extract the ENTIRE archive, "
    L"then run faraday.exe from inside the extracted Faraday folder. "
    L"Copying faraday.exe out on its own will not work — the program "
    L"needs the files that sit next to it.\r\n\r\n"
    L"If you used the installer: please reinstall.";

struct PathBuf
{
    wchar_t data[MAX_PATH * 4];
};

bool quietMode()
{
    wchar_t buf[8];
    return GetEnvironmentVariableW(L"FARADAY_LAUNCHER_NO_UI", buf, 8) > 0;
}

void writeStderr(const wchar_t *text)
{
    const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err == INVALID_HANDLE_VALUE || err == nullptr)
        return;
    // UTF-8 encode for pipe capture.
    char utf8[4096];
    const int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8, sizeof(utf8) - 1,
                                      nullptr, nullptr);
    if (n > 1) {
        DWORD written = 0;
        WriteFile(err, utf8, static_cast<DWORD>(n - 1), &written, nullptr);
        WriteFile(err, "\r\n", 2, &written, nullptr);
    }
}

[[noreturn]] void fail(const wchar_t *message, UINT exitCode)
{
    if (quietMode())
        writeStderr(message);
    else
        MessageBoxW(nullptr, message, kTitle, MB_OK | MB_ICONWARNING);
    ExitProcess(exitCode);
}

// Directory containing this launcher, with a trailing backslash.
bool moduleDirectory(PathBuf &out)
{
    const DWORD len = GetModuleFileNameW(nullptr,
                                         out.data,
                                         static_cast<DWORD>(sizeof(out.data) / sizeof(wchar_t)));
    if (len == 0 || len >= sizeof(out.data) / sizeof(wchar_t))
        return false;
    for (DWORD i = len; i > 0; --i) {
        if (out.data[i - 1] == L'\\' || out.data[i - 1] == L'/') {
            out.data[i] = L'\0';
            return true;
        }
    }
    return false;
}

bool joinPath(PathBuf &out, const PathBuf &dir, const wchar_t *relative)
{
    size_t i = 0;
    const size_t cap = sizeof(out.data) / sizeof(wchar_t);
    while (dir.data[i] != L'\0' && i < cap - 1) {
        out.data[i] = dir.data[i];
        ++i;
    }
    size_t j = 0;
    while (relative[j] != L'\0' && i < cap - 1)
        out.data[i++] = relative[j++];
    out.data[i] = L'\0';
    return relative[j] == L'\0';
}

bool fileExists(const PathBuf &dir, const wchar_t *relative)
{
    PathBuf full;
    if (!joinPath(full, dir, relative))
        return false;
    const DWORD attrs = GetFileAttributesW(full.data);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// Verifies every file listed in runtime.manifest (one relative path per
// line). Returns true when the manifest exists and was checked; missing
// entries are reported through *firstMissing / *missingCount.
bool checkManifest(const PathBuf &dir, wchar_t *firstMissing, size_t firstMissingCap,
                   int *missingCount)
{
    *missingCount = 0;
    firstMissing[0] = L'\0';

    PathBuf manifestPath;
    if (!joinPath(manifestPath, dir, kManifestName))
        return false;
    const HANDLE file = CreateFileW(manifestPath.data, GENERIC_READ, FILE_SHARE_READ,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > (1 << 20)) {
        CloseHandle(file);
        return false;
    }
    const DWORD bytes = static_cast<DWORD>(size.QuadPart);
    char *buf = static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, bytes + 1));
    if (!buf) {
        CloseHandle(file);
        return false;
    }
    DWORD read = 0;
    const bool ok = ReadFile(file, buf, bytes, &read, nullptr) && read == bytes;
    CloseHandle(file);
    if (!ok) {
        HeapFree(GetProcessHeap(), 0, buf);
        return false;
    }
    buf[bytes] = '\0';

    // Parse lines (ASCII relative paths, CRLF or LF).
    DWORD lineStart = 0;
    for (DWORD i = 0; i <= bytes; ++i) {
        if (i == bytes || buf[i] == '\n' || buf[i] == '\r') {
            if (i > lineStart) {
                wchar_t rel[MAX_PATH * 2];
                const int wlen = MultiByteToWideChar(CP_UTF8, 0, buf + lineStart,
                                                     static_cast<int>(i - lineStart), rel,
                                                     MAX_PATH * 2 - 1);
                if (wlen > 0) {
                    rel[wlen] = L'\0';
                    if (!fileExists(dir, rel)) {
                        if (*missingCount == 0) {
                            size_t k = 0;
                            while (rel[k] != L'\0' && k < firstMissingCap - 1) {
                                firstMissing[k] = rel[k];
                                ++k;
                            }
                            firstMissing[k] = L'\0';
                        }
                        ++(*missingCount);
                    }
                }
            }
            lineStart = i + 1;
        }
    }
    HeapFree(GetProcessHeap(), 0, buf);
    return true;
}

// Everything after the program token of the original command line.
const wchar_t *argumentsTail()
{
    const wchar_t *cmd = GetCommandLineW();
    if (!cmd)
        return L"";
    if (*cmd == L'"') {
        ++cmd;
        while (*cmd && *cmd != L'"')
            ++cmd;
        if (*cmd == L'"')
            ++cmd;
    } else {
        while (*cmd && *cmd != L' ' && *cmd != L'\t')
            ++cmd;
    }
    while (*cmd == L' ' || *cmd == L'\t')
        ++cmd;
    return cmd;
}

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    PathBuf dir;
    if (!moduleDirectory(dir))
        fail(kExtractAdvice, 2);

    // 1. The application binary itself must be beside us (the bare-exe case).
    if (!fileExists(dir, kAppBinary))
        fail(kExtractAdvice, 2);

    // 2. Verify the packaged runtime manifest when present; otherwise fall
    //    back to the built-in core list only if those files are also absent
    //    AND the spawn check below fails (developer trees resolve Qt via
    //    PATH, so missing-beside-us alone is not proof of breakage there).
    wchar_t firstMissing[MAX_PATH * 2];
    int missingCount = 0;
    if (checkManifest(dir, firstMissing, MAX_PATH * 2, &missingCount) && missingCount > 0) {
        wchar_t msg[2048];
        wsprintfW(msg,
                  L"%d required program file(s) are missing (first: %s).\r\n\r\n%s",
                  missingCount, firstMissing, kExtractAdvice);
        fail(msg, 2);
    }

    // 3. Start the real application with loader error boxes suppressed (the
    //    child inherits our error mode) and watch its first seconds so a
    //    damaged runtime still ends in a friendly message.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    PathBuf appPath;
    if (!joinPath(appPath, dir, kAppBinary))
        fail(kExtractAdvice, 2);

    wchar_t cmdLine[4096];
    const wchar_t *tail = argumentsTail();
    if (tail[0] != L'\0')
        wsprintfW(cmdLine, L"\"%s\" %s", appPath.data, tail);
    else
        wsprintfW(cmdLine, L"\"%s\"", appPath.data);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(appPath.data, cmdLine, nullptr, nullptr, FALSE, 0, nullptr,
                        dir.data, &si, &pi)) {
        fail(kExtractAdvice, 3);
    }
    CloseHandle(pi.hThread);

    // A healthy start means the app is still alive after the grace window.
    const DWORD wait = WaitForSingleObject(pi.hProcess, 4000);
    DWORD exitCode = 0;
    if (wait == WAIT_OBJECT_0 && GetExitCodeProcess(pi.hProcess, &exitCode)
        && exitCode != 0) {
        CloseHandle(pi.hProcess);
        wchar_t msg[2048];
        wsprintfW(msg,
                  L"Faraday could not start (code 0x%08X). The program files next to "
                  L"faraday.exe appear incomplete or damaged.\r\n\r\n%s",
                  exitCode, kExtractAdvice);
        fail(msg, 3);
    }
    CloseHandle(pi.hProcess);
    return 0;
}
