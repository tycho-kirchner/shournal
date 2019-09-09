#pragma once

#include <QDir>

#include "fileinfos.h"

class StoredFiles
{
public:

    static const QString &getReadFilesDir();

    static const QString &mkpath();

    StoredFiles();

    QString mkPathStringToStoredReadFile(const FileReadInfo& info);

    bool deleteReadFile(const QString& fname);

    void addReadFile(const QString& fname, const QByteArray& data);

    void restoreReadFileAtDIr(const FileReadInfo &info, const QDir& dir,
                                const QFile &openReadFileInDb);

    void restoreReadFileAtDIr(const FileReadInfo &info, const QDir& dir);

private:
    QDir m_readFilesDir;

};

