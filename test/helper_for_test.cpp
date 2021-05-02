
#include <QStandardPaths>
#include <QCoreApplication>
#include <QDebug>
#include <QTextStream>

#include "helper_for_test.h"
#include "util.h"
#include "app.h"
#include "exccommon.h"
#include "qfilethrow.h"

namespace  {

const QList<QStandardPaths::StandardLocation>& locations(){
    static const QList<QStandardPaths::StandardLocation> locs = {
        QStandardPaths::ConfigLocation,
        QStandardPaths::DataLocation,
        QStandardPaths::CacheLocation};
    return locs;
}


} //  namespace

/// Set application-name in a unique way and enable test mode in QStandardPaths,
/// so application stuff is saved somewhere else. In the end, remove the
/// respective directories (cleanupPaths).
void testhelper::setupPaths()
{
    for(const auto& l : locations()){
        const QString path = QStandardPaths::writableLocation(l);
        QDir d(path);
        if( ! d.mkpath(path)){
            throw QExcIo(QString("Failed to create %1").arg(path));
        }
    }
}


void testhelper::deletePaths()
{
    if(! QStandardPaths::isTestModeEnabled()){
        throw QExcProgramming(QString(__func__) + " called while test mode disabled");
    }
    for(const auto& l : locations()){
        const QString path = QStandardPaths::writableLocation(l);
        QDir d(path);
        d.removeRecursively();
    }
}

void testhelper::deleteDatabaseDir()
{
    if(! QStandardPaths::isTestModeEnabled()){
        throw QExcProgramming(QString(__func__) + " called while test mode disabled");
    }
    const QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QDir d(path);
    d.removeRecursively();
}


std::shared_ptr<QTemporaryDir> testhelper::mkAutoDelTmpDir()
{
    auto pDir = std::make_shared<QTemporaryDir>();
    if (! pDir->isValid()) {
         throw QExcIo("Failed to mk temp dir");
    }
    pDir->setAutoRemove(true);
    return pDir;
}

void testhelper::writeStringToFile(const QString &filepath, const QString &str)
{
    QFileThrow f(filepath);
    f.open(QFile::WriteOnly | QFile::Text);

    QTextStream stream(&f);
    stream << str;
}

/// Write repeated string pattern of len to the file at path
void testhelper::writeStuffToFile(const QString &fpath, int len){
    const QByteArray stuff("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    QFileThrow f(fpath);
    f.open(QFile::WriteOnly | QFile::Text);
    for(int i=0; i < len / stuff.size(); i++){
        f.write(stuff);
    }
    int rest = len % stuff.size();
    if(rest){
        auto stuffrest = QByteArray::fromRawData(stuff, rest);
        f.write(stuffrest);
    }

}

QString testhelper::readStringFromFile(const QString &fpath)
{
    QFileThrow f(fpath);
    f.open(QFile::ReadOnly | QFile::Text);

    QTextStream stream(&f);
    return stream.readAll();
}


bool testhelper::copyRecursively(const QString &srcFilePath,
                                 const QString &tgtFilePath)
{
    QFileInfo srcFileInfo(srcFilePath);
    if (srcFileInfo.isDir()) {
        QDir targetDir(tgtFilePath);
        targetDir.cdUp();
        if (!targetDir.mkdir(QFileInfo(tgtFilePath).fileName()))
            return false;
        QDir sourceDir(srcFilePath);
        QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        foreach (const QString &fileName, fileNames) {
            const QString newSrcFilePath
                    = srcFilePath + QLatin1Char('/') + fileName;
            const QString newTgtFilePath
                    = tgtFilePath + QLatin1Char('/') + fileName;
            if (!copyRecursively(newSrcFilePath, newTgtFilePath))
                return false;
        }
    } else {
        if (!QFile::copy(srcFilePath, tgtFilePath))
            return false;
    }
    return true;
}
