#include "PortableMode.h"

#include <QDir>
#include <QFileInfo>

namespace faraday {

QString PortableMode::markerFileName()
{
    return QStringLiteral("portable.txt");
}

QString PortableMode::dataDirFor(const QString &exeDir)
{
    if (exeDir.isEmpty())
        return QString();
    QDir dir(exeDir);
    if (!QFileInfo::exists(dir.filePath(markerFileName())))
        return QString();

    // Data belongs beside the launcher (the program root), not inside the
    // runtime folder. cdUp() fails only at a filesystem root, where the
    // exe directory IS the program root — fall back to it in that case.
    QDir root(exeDir);
    if (!root.cdUp())
        return dir.filePath(QStringLiteral("data"));
    return root.filePath(QStringLiteral("data"));
}

} // namespace faraday
