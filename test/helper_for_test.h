#pragma once

#include <QTemporaryDir>
#include <memory>

namespace testhelper {
void setupPaths();
void deletePaths();
void deleteDatabaseDir();

std::shared_ptr<QTemporaryDir> mkAutoDelTmpDir();

void writeStringToFile(const QString& filepath, const QString& str);
QString readStringFromFile(const QString& fpath);

}



