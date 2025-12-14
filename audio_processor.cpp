#include "audio_processor.h"
#include <QFileInfo>
#include <QString>

QString AudioProcessor::summarizeWav(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return QString("File not found: %1").arg(path);
    }
    const double sizeKb = info.size() / 1024.0;
    return QString("%1 (%.1f KB)").arg(info.fileName()).arg(sizeKb);
}
