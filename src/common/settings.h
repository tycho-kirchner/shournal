
#pragma once

#include <QString>
#include <unordered_set>
#include <QVersionNumber>
#include <memory>

#include "hashmeta.h"
#include "pathtree.h"
#include "cfg.h"
#include "qfilethrow.h"

using std::make_shared;

class SafeFileUpdate;

class Settings {
public:
    typedef std::unordered_set<std::string> StringSet;
    typedef std::unordered_set<StrLight> StrLightSet;
    typedef std::unordered_set<QString> MimeSet;

    static Settings & instance();

    struct HashSettings {
        HashMeta hashMeta;
        bool hashEnable{};
    };

    struct WriteFileSettings {
        WriteFileSettings() :
            includePaths(make_shared<PathTree>()),
            includePathsHidden(make_shared<PathTree>()),
            excludePaths(make_shared<PathTree>()) {}

        std::shared_ptr<PathTree> includePaths;
        std::shared_ptr<PathTree> includePathsHidden;
        std::shared_ptr<PathTree> excludePaths;
        bool onlyClosedWrite {true};
        bool excludeHidden {true};
        uint64_t maxEventCount{std::numeric_limits<uint64_t>::max()};

        Q_DISABLE_COPY(WriteFileSettings)
        DISABLE_MOVE(WriteFileSettings)
    };

    struct ReadFileSettings {
        ReadFileSettings() :
        includePaths(make_shared<PathTree>()),
        includePathsHidden(make_shared<PathTree>()),
        excludePaths(make_shared<PathTree>()) {}

        bool enable {true};
        std::shared_ptr<PathTree> includePaths;
        std::shared_ptr<PathTree> includePathsHidden;
        std::shared_ptr<PathTree> excludePaths;
        bool onlyWritable {true};
        bool excludeHidden {true};
        uint64_t maxEventCount{std::numeric_limits<uint64_t>::max()};

        Q_DISABLE_COPY(ReadFileSettings)
        DISABLE_MOVE(ReadFileSettings)
    };

    /// Holds settings for read files which shall be stored to disk
    /// ( probably mostly scripts or similar files).
    struct ScriptFileSettings {
        ScriptFileSettings() :
            includePaths(make_shared<PathTree>()),
            includePathsHidden(make_shared<PathTree>()),
            excludePaths(make_shared<PathTree>()) {}

        // store read files to disk, if...
        bool enable {false}; // .. enabled
        bool onlyWritable {true}; // .. user has write permission
        bool excludeHidden {true}; // .. it is not hidden
        std::shared_ptr<PathTree> includePaths; // .. it is equal to or below an include path
        std::shared_ptr<PathTree> includePathsHidden; // .. see above
        std::shared_ptr<PathTree> excludePaths; // .. it is not equal to or below an exclude path
        StrLightSet includeExtensions; // .. file extension, mimetype matches ( it's
        MimeSet includeMimetypes;    //   more complicated than that)
        qint64 maxFileSize {500*1024}; // .. it's not bigger than this size
        uint maxCountOfFiles {3}; // .. we have not already collected that many read files

        int flushToDiskTotalSize {1024*1024*10}; // read files (scripts) are cached in memory. If their total size is
                                  // greater than that, flush to disk (database)

        Q_DISABLE_COPY(ScriptFileSettings)
        DISABLE_MOVE(ScriptFileSettings)
    };



public:
    void load();
    QString chooseShournalRunBackend();

    const HashSettings& hashSettings() const;
    const WriteFileSettings& writeFileSettings() const;
    const ReadFileSettings& readFileSettings() const;
    const ScriptFileSettings& readEventScriptSettings() const;

    QString cfgAppDir();
    QString cfgFilepath();

    const QStringList& defaultIgnoreCmds();

    const StringSet& ignoreCmds();
    const StringSet& ignoreCmdsRegardslessOfArgs();

    const StrLightSet &getMountIgnorePaths();
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

    ReadVersionReturn readVersion(SafeFileUpdate &verUpd8);
    void handleUnequalVersions(ReadVersionReturn& readVerResult);
    void storeCfg(SafeFileUpdate& cfgUpd8, SafeFileUpdate& verUpd8);
    std::shared_ptr<PathTree> loadPaths(qsimplecfg::Cfg::Section_Ptr& section,
              const QString& keyName, bool eraseSubpaths,
              const std::unordered_set<QString> & defaultValues,
              PathTree* hiddenPaths=nullptr);

    qsimplecfg::Cfg m_cfg;
    HashSettings m_hashSettings;
    WriteFileSettings m_wSettings;
    ReadFileSettings m_rSettings;
    ScriptFileSettings m_scriptSettings;
    StrLightSet m_mountIgnorePaths;
    bool m_mountIgnoreNoPerm {false};
    bool m_settingsLoaded {false};
    StringSet m_ignoreCmds;
    StringSet m_ignoreCmdsRegardlessOfArgs;
    const QString m_userHome { QDir::homePath() };
    const QString m_workingDir { QDir::currentPath() };
    QVersionNumber m_parsedCfgVersion;

private:
    // unit testing...
    friend class FileEventHandlerTest;
    friend class IntegrationTestShell;
    friend class GeneralTest;
};


