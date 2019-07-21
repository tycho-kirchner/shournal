#pragma once

#include <QString>

namespace console_dialog {

bool yesNo(const QString& question);

int openFileInExternalEditor(const QString& filepath);

}
