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
    const QDir dir(exeDir);
    if (!QFileInfo::exists(dir.filePath(markerFileName())))
        return QString();
    return dir.filePath(QStringLiteral("data"));
}

} // namespace faraday
