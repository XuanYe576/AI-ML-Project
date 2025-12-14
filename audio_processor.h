#pragma once
#include <QString>

class AudioProcessor {
public:
    // Simple placeholder: reports file size; extend to real DSP later.
    static QString summarizeWav(const QString &path);
};
