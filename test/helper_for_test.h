#pragma once

#include <QTemporaryDir>
#include <memory>

namespace testhelper {
void setupPaths();
void deletePaths();

std::shared_ptr<QTemporaryDir> mkAutoDelTmpDir();

}



