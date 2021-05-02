#pragma once

#include <QTemporaryDir>
#include <memory>

namespace testhelper {
void setupPaths();
void deletePaths();
void deleteDatabaseDir();

std::shared_ptr<QTemporaryDir> mkAutoDelTmpDir();

void writeStringToFile(const QString& filepath, const QString& str);
void writeStuffToFile(const QString &fpath, int len);

QString readStringFromFile(const QString& fpath);

bool copyRecursively(const QString &srcFilePath,
                     const QString &tgtFilePath);

}



