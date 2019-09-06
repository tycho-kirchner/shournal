
#pragma once

#include <QString>
#include <unordered_set>
#include <QVersionNumber>

#include "hashmeta.h"
#include "pathtree.h"
#include "cfg.h"
#include "qfilethrow.h"



class Settings {
public:
    typedef std::unordered_set<std::string> StringSet;
    typedef std::unordered_set<QString> MimeSet;

    static Settings & instance();

    struct HashSettings {
        HashMeta hashMeta;
        bool hashEnable{};
    };

    struct WriteFileSettings {
        PathTree includePaths;
        PathTree includePathsHidden;
        PathTree excludePaths;
        bool onlyClosedWrite {true};
        bool excludeHidden {true};
        int flushToDiskEventCount {2000};
    };

    struct ReadFileSettings {
        bool enable {true};
        PathTree includePaths;
        PathTree includePathsHidden;
        PathTree excludePaths;
        bool onlyWritable {true};
        bool excludeHidden {true};
        int flushToDiskEventCount {2000};
    };

    /// Holds settings for read files which shall be stored to disk
    /// ( probably mostly scripts or similar files).
    struct ScriptFileSettings {
        // store read files to disk, if...
        bool enable {false}; // .. enabled
        bool onlyWritable {true}; // .. user has write permission
        PathTree includePaths; // .. it is equal to or below an include path
        PathTree excludePaths; // .. it is not equal to or below an exclude path
        StringSet includeExtensions; // .. file extension, mimetype matches ( it's
        MimeSet includeMimetypes;    //   more complicated than that)
        qint64 maxFileSize {500*1024}; // .. it's not bigger than this size
        int maxCountOfFiles {3}; // .. we have not already collected that many read files

        int flushToDiskTotalSize {1024*1024*10}; // read files (scripts) are cached in memory. If their total size is
                                  // greater than that, flush to disk (database)
    };



public:
    void load();

    const HashSettings& hashSettings() const;
    const WriteFileSettings& writeFileSettings() const;
    const ReadFileSettings& readFileSettins() const;
    const ScriptFileSettings& readEventScriptSettings() const;

    QString cfgFilepath();

    const QStringList& defaultIgnoreCmds();

    const StringSet& ignoreCmds();
    const StringSet& ignoreCmdsRegardslessOfArgs();

    const StringSet& getMountIgnorePaths();
    bool getMountIgnoreNoPerm() const;

public:
    ~Settings() = default;
    Q_DISABLE_COPY(Settings)
    DISABLE_MOVE(Settings)

public:
    static const char* SECT_READ_NAME;
    static const char* SECT_READ_KEY_ENABLE;
    static const char* SECT_READ_KEY_INCLUDE_PATHS;

    static const char* SECT_SCRIPTS_NAME;
    static const char* SECT_SCRIPTS_ENABLE;
    static const char* SECT_SCRIPTS_INCLUDE_PATHS;
    static const char* SECT_SCRIPTS_INCLUDE_FILE_EXTENSIONS;

private:
    struct ReadVersionReturn {
        QVersionNumber ver;
        bool verLoadedFromLegacyPath{};
        QString verFilePath;
    };

    Settings() = default;
    void addIgnoreCmd(QString cmd, bool warnIfNotFound, const QString & ignoreCmdsSectName);
    void loadSections();
    void loadSectWrite();
    void loadSectRead();
    void loadSectScriptFiles();
    void loadSectIgnoreCmd();
    void loadSectMount();
    void loadSectHash();

    bool parseCfgIfExists(const QString &cfgPath);
    ReadVersionReturn readVersion(QFileThrow &cfgVersionFile);
    void handleUnequalVersions(ReadVersionReturn& readVerResult);
    void storeCfg(QFileThrow& cfgVersionFile);
    PathTree loadPaths(qsimplecfg::Cfg::Section_Ptr& section,
              const QString& keyName, bool eraseSubpaths,
              const std::unordered_set<QString> & defaultValues,
              PathTree* hiddenPaths=nullptr);

    qsimplecfg::Cfg m_cfg;
    HashSettings m_hashSettings;
    WriteFileSettings m_wSettings;
    ReadFileSettings m_rSettings;
    ScriptFileSettings m_scriptSettings;
    StringSet m_mountIgnorePaths;
    bool m_mountIgnoreNoPerm {false};
    bool m_settingsLoaded {false};
    StringSet m_ignoreCmds;
    StringSet m_ignoreCmdsRegardlessOfArgs;
    const QString m_userHome { QDir::homePath() };
    const QString m_workingDir { QDir::currentPath() };

private:
    // unit testing...
    friend class FileEventHandlerTest;
    friend class IntegrationTestShell;
};


