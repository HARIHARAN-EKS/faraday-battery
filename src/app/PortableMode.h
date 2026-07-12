#pragma once

#include <QString>

namespace faraday {

// Zero-install distribution support.
//
// When a marker file named "portable.txt" sits next to faraday.exe, all
// application data (settings.json + faraday.sqlite) lives in <exeDir>\data
// instead of %APPDATA%, so the whole application travels with its folder —
// USB drives included. Installed copies never contain the marker, so their
// behavior is unchanged. No registry, no environment variables, no
// elevation: the marker is just a file.
class PortableMode
{
public:
    // Name of the marker file shipped at the root of the portable ZIP.
    static QString markerFileName();

    // Returns "<exeDir>/data" when the marker exists in exeDir, otherwise
    // an empty string (meaning: use the default per-user data location).
    // Pure filesystem inspection; never creates anything.
    static QString dataDirFor(const QString &exeDir);
};

} // namespace faraday
