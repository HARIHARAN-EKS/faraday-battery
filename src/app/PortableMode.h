#pragma once

#include <QString>

namespace faraday {

// Zero-install distribution support.
//
// Layout (P2): the Qt application (faraday-core.exe) lives in the "app"
// subfolder of the program directory; the top level holds only Faraday.exe
// and README.txt. The portable marker ships as app\portable.txt (out of
// sight), and when it is present ALL user data lives in the TOP-LEVEL
// data folder — deliberately outside app\, so user data is never mixed
// with the runtime and survives a drop-in replacement of app\:
//
//   Faraday/
//     Faraday.exe
//     README.txt
//     data/            <- settings.json + faraday.sqlite (created on first run)
//     app/
//       faraday-core.exe, portable.txt, Qt runtime...
//
// Installed copies ship no marker, so they use the standard per-user data
// location instead. No registry, no environment variables, no elevation:
// the marker is just a file.
class PortableMode
{
public:
    // Name of the marker file shipped inside the app directory.
    static QString markerFileName();

    // exeDir is the directory containing faraday-core.exe (i.e. ...\app).
    // Returns "<parent of exeDir>/data" when the marker exists in exeDir,
    // otherwise an empty string (meaning: use the default per-user data
    // location). Pure filesystem inspection; never creates anything.
    static QString dataDirFor(const QString &exeDir);
};

} // namespace faraday
