
#pragma once

#include <QString>
#include <unordered_set>

#include "hashmeta.h"
#include "pathtree.h"
#include "cfg.h"



class Settings {
public:
    typedef std::unordered_set<std::string> StringSet;
    typedef std::unordered_set<QString> MimeSet;

    static Settings & instance();

    struct ReadFileSettings {
        ReadFileSettings();

        // only save read files, if...
        bool enable; // .. enabled
        bool onlyWritable; // .. user has write permission
        PathTree includePaths; // .. it is equal to or below an include path
        PathTree excludePaths; // .. it is not equal to or below an exclude path
        StringSet includeExtensions; // .. file extension, mimetype matches ( it's
        MimeSet includeMimetypes;    //   more complicated than that)
        qint64 maxFileSize; // .. it's not bigger than this size
        int maxCountOfFiles; // .. we have not already collected that many read files

        int flushToDiskTotalSize; // read files (scripts) are cached in memory. If their total size is
                                  // greater than that, flush to disk (database)
    };

    struct WriteFileSettings {
        WriteFileSettings();

        HashMeta hashMeta;
        bool hashEnable;
        PathTree includePaths;
        PathTree excludePaths;
        bool onlyClosedWrite;
        int flushToDiskEventCount;
    };

public:
    void load(const QString & path=QString());

    const ReadFileSettings& readEventSettings();
    const WriteFileSettings& writeFileSettings();

    QString defaultCfgFilepath();

    const QStringList& defaultIgnoreCmds();

    const StringSet& ignoreCmds();
    const StringSet& ignoreCmdsRegardslessOfArgs();

    const QString& lastLoadedFilepath();

    const StringSet& getMountIgnorePaths();
    bool getMountIgnoreNoPerm() const;

public:
    Settings(const Settings &) = delete ;
    void operator=(const Settings &) = delete ;


private:
    Settings();
    void addIgnoreCmd(QString cmd, bool warnIfNotFound, const QString & ignoreCmdsSectName);

    qsimplecfg::Cfg m_cfg;
    ReadFileSettings m_rSettings;
    WriteFileSettings m_wSettings;
    StringSet m_mountIgnorePaths;
    bool m_mountIgnoreNoPerm;
    bool m_settingsLoaded;
    StringSet m_ignoreCmds;
    StringSet m_ignoreCmdsRegardlessOfArgs;

private:
    // unit testing...
    friend class FileEventHandlerTest;
};


