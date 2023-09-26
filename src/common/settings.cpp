
#include <cassert>
#include <QStandardPaths>
#include <QVariant>
#include <QDebug>
#include <QCoreApplication>
#include <regex>


#include "settings.h"

#include "cfg.h"
#include "exccfg.h"
#include "os.h"
#include "util.h"
#include "pathtree.h"
#include "logger.h"
#include "app.h"
#include "translation.h"
#include "cflock.h"
#include "qfilethrow.h"
#include "conversions.h"
#include "safe_file_update.h"

using Section_Ptr = qsimplecfg::Cfg::Section_Ptr;
using qsimplecfg::ExcCfg;
using StringSet = Settings::StringSet;
using std::numeric_limits;

const char* Settings::SECT_READ_NAME {"File read-events"};
const char* Settings::SECT_READ_KEY_ENABLE {"enable"};
const char* Settings::SECT_READ_KEY_INCLUDE_PATHS {"include_paths"};


const char* Settings::SECT_SCRIPTS_NAME {"File read-events storage settings"};
const char* Settings::SECT_SCRIPTS_ENABLE {"enable"};
const char* Settings::SECT_SCRIPTS_INCLUDE_PATHS {"include_paths"};
const char* Settings::SECT_SCRIPTS_INCLUDE_FILE_EXTENSIONS {"include_file_extensions"};


Settings &Settings::instance()
{
    static Settings s;
    return s;
}


const QStringList &Settings::defaultIgnoreCmds()
{
    // commands ending with an asterisk will be
    // later inserted into the to-ignore-commands
    // No args may be added in that case.
    static const QStringList vals = {"mount*",
                                     QString(app::SHOURNAL) + '*',
                                     QString(app::SHOURNAL_RUN) + '*'
                                         };
    return vals;
}

QString Settings::cfgAppDir()
{
    // don't make path static -> mutliple test cases...
    return
       pathJoinFilename(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation),
                        QCoreApplication::applicationName());
}

QString Settings::cfgFilepath()
{
    // don't make path static -> mutliple test cases...
    return cfgAppDir() + "/config.ini";
}

// was in use until shournal 2.1, then migrated
// from .cache/shournal to .config/shournal
static QString legacyCfgVersionFilePath(){

    // don't make path static -> mutliple test cases...
    const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + "/config-file-version";
    return path;
}


/// For lines not ending with an asterisk:
/// The first word (whitespace!) is considered the command, whose full
/// path is found, the rest is appended as arguments.
/// If no command could be found, the returned string is empty
/// Example
/// "bash -c"  -> /bin/bash -c
/// "bash" -> /bin/bash
static QString ignoreCmdLineToFullCmdAndArgs(const QString& str){
    int spaceIdx = str.indexOf(QChar::Space);
    if(spaceIdx == -1){
        return  QStandardPaths::findExecutable(str);
    }
    QString cmd = str.left(spaceIdx);
    cmd = QStandardPaths::findExecutable(cmd);
    if(cmd.isEmpty()) return cmd;
    // space still there...:
    return cmd + str.mid(spaceIdx);

}


/// Adds a given command string from defaults or config file to the respective set.
/// @param warnIfNotFound: if false, no warnings are printed if a command is not found.
/// It is to prevenet printing warnings for default commands, which are not installed on
/// the target system.
/// If a command ends with an asterisk, it will be ignored, regardless
/// of its arguments. Else the commands are only ignored it the argument match exactly
void Settings::addIgnoreCmd(QString cmd, bool warnIfNotFound, const QString & ignoreCmdsSectName){
    const QString lineCopy = cmd;
    // do not simplify as argmument in the config parser!
    cmd=cmd.simplified();
    const QString ignoreCmdsErrPreamble = ignoreCmdsSectName +
            qtr(": invalid command in line ") + '<';
    if(cmd.endsWith('*')){
        cmd.remove(cmd.size()-1, 1);
        cmd=cmd.trimmed();
        if(cmd.contains(' ')){
            logWarning << ignoreCmdsErrPreamble << lineCopy << "> - "
                       << qtr("The command contains whitespaces. "
                              "Note that arguments are not (yet) supported "
                              "when wildcards are used.");

            return;
        }
        QString fullPath = QStandardPaths::findExecutable(cmd);
        if(fullPath.isEmpty()){
            if(warnIfNotFound){
                logWarning << ignoreCmdsErrPreamble << lineCopy << "> - not found:"  << cmd;
            }
        } else {
            m_ignoreCmdsRegardlessOfArgs.insert(fullPath.toStdString());
        }
    } else {
        cmd = ignoreCmdLineToFullCmdAndArgs(cmd);
        if(cmd.isEmpty()){
            if(warnIfNotFound){
                logWarning << ignoreCmdsErrPreamble << lineCopy << "> - not found." ;
            }
        } else {

            m_ignoreCmds.insert(cmd.toStdString());
        }
    }
}




/// @param hiddenPaths: if not null, store hidden paths in the passed tree, instead
/// of the returned one.
std::shared_ptr<PathTree> Settings::loadPaths(Section_Ptr& section,
          const QString& keyName,
          bool eraseSubpaths,
          const std::unordered_set<QString> & defaultValues,
          PathTree* hiddenPaths){
    auto rawPaths = section->getValues<std::unordered_set<QString> >(keyName,
                                              defaultValues,
                                              false, "\n");
    auto tree = std::make_shared<PathTree>();
    for(const auto& p : rawPaths){
        QString canonicalPath = p;
        if(canonicalPath.startsWith("$CWD")){
            if(m_workingDir.isEmpty()){
                logWarning << qtr("section %1: %2: $CWD is set but the working-"
                                  "directory could not be determined. Maybe it does "
                                  "not exist?")
                              .arg(section->sectionName(), keyName);
                continue;
            }
            canonicalPath.replace("$CWD", m_workingDir);

        } else if(canonicalPath.startsWith("$HOME")){
            canonicalPath.replace("$HOME", m_userHome);
        } else if(canonicalPath.startsWith("~")) {
            canonicalPath.replace("~", m_userHome);
        }

        canonicalPath = QDir(canonicalPath).canonicalPath();
        if(canonicalPath.isEmpty()){
            logWarning << qtr("section %1: %2: path does not exist: %3")
                          .arg(section->sectionName(), keyName, p);
            continue;
        }

        PathTree* currentTree =(hiddenPaths != nullptr &&
                                canonicalPath.contains("/.")) ? hiddenPaths : tree.get();
        auto canoicalPathLight = toStrLight(canonicalPath);
        // avoid adding needless parent/subpaths
        if(eraseSubpaths){
            if(currentTree->isSubPath(canoicalPathLight)){
                logDebug<< keyName << "ignore" << canonicalPath
                         << "because it is a subpath";
                continue;

            }
            // the new path might be a parent path:
            // erase its children (if any)
            auto subPathIt = currentTree->subpathIter(canoicalPathLight);
            while(subPathIt != currentTree->end() ){
                logDebug << keyName << "ignore" << *subPathIt
                         << "because it is a subpath";
                subPathIt = currentTree->erase(subPathIt);
            }
        }
        currentTree->insert(canoicalPathLight);
    }
    return tree;
}




/// Until shournal 2.1 the version file was located at .cache, then migrated
/// to .config/shournal
/// @return QVersionNumer.isNull==true In case of no or an invalid version.
static QVersionNumber readLegacyConfigFileVersion(){
    const QString path = legacyCfgVersionFilePath();
    QFile f(path);
    if(! f.open(QFile::OpenModeFlag::ReadOnly)){
        return {};
    }    
    CFlock l(f.handle());
    l.lockShared();
    auto ver = QVersionNumber::fromString(QTextStream(&f).readLine());
    l.unlock(); // unlock explicitly: in case f is removed, it is closed beforehand,
                // so the fd passed to CFlock is already invalid.
    if(ver.isNull()){
        logWarning << QString("Bad version string in file %1. Deleting it...")
                      .arg(path);
        f.remove();
    }
    return ver;
}



/// Remove all paths from excludePaths which are not sub-paths
/// of any tree in includePathtrees. Print a warning in this case.
static void cleanExcludePaths(const QVector<const PathTree*>& includePathtrees,
                              std::shared_ptr<PathTree>& excludePaths,
                              const QString& sectionName){
    for(auto it=excludePaths->begin(); it != excludePaths->end();){
        bool isSubPath = false;
        for(const PathTree* includePaths : includePathtrees){
            if(includePaths->isSubPath(*it)){
                isSubPath = true;
                break;
            }
        }
        if(isSubPath){
            ++it;
        } else {
            logWarning << qtr("section %1: ignore exclude-path %2 - it is not a sub-path "
                              "of any include-path").arg(sectionName).arg((*it).c_str());
            it = excludePaths->erase(it);
        }
    }
}

// static void cleanExcludePaths(const PathTree& includePaths, PathTree& excludePaths,
//                                  const QString& sectionName){
//     cleanExcludePaths( {&includePaths}, excludePaths, sectionName);
// }

static void cleanExcludePaths(const std::shared_ptr<PathTree>& includePaths,
                              const PathTree* optionalIncludePaths,
                              std::shared_ptr<PathTree>& excludePaths,
                              const QString& sectionName){
    if(optionalIncludePaths != nullptr){
        cleanExcludePaths( {includePaths.get(), optionalIncludePaths}, excludePaths, sectionName);
    }
    cleanExcludePaths( {includePaths.get()}, excludePaths, sectionName);
}

void Settings::loadSections(){
    m_cfg.setInitialComments(qtr(
                                 "Configuration file for %1. Uncomment lines "
                                 "to change defaults. Multi-line-values (e.g. paths) "
                                 "are framed by leading and trailing "
                                 "triple-quotes ''' .\n"
                                 "When loading paths, the following symbols may be "
                                 "specified:\n"
                                 "$HOME or ~ for your home directory\n"
                                 "$CWD for the current working directory\n"
                                 "In several sections, the key 'exclude_hidden'\n"
                                 "can be set to true - in this case, a "
                                 "file event is excluded, if it is below "
                                 "*any* hidden directory or is hidden itself. A "
                                 "explicitly included hidden file is not affected.\n"
                                 "Please do not store custom comments in this file, "
                                 "as those are lost each time shournal is updated to "
                                 "a new version.").arg(app::SHOURNAL)
                             );
    loadSectWrite();
    loadSectRead();
    loadSectScriptFiles();
    loadSectIgnoreCmd();
    loadSectMount();
    loadSectHash();
}

void Settings::loadSectWrite()
{
    auto sectWriteEvents = m_cfg["File write-events"];
    sectWriteEvents->setComments(qtr(
                                     "Configure, which paths shall be observed for "
                                     "*write*-events. Put each desired path into "
                                     "a separate line. "
                                     "Default is to observe all paths.\n"
                                     ));
    m_wSettings.excludeHidden = sectWriteEvents->getValue<bool>("exclude_hidden", true);
    PathTree* hiddenPaths = (m_wSettings.excludeHidden) ? m_wSettings.includePathsHidden.get() : nullptr;
    m_wSettings.includePaths = loadPaths(
                sectWriteEvents, "include_paths", true, {"/"}, hiddenPaths);
    m_wSettings.excludePaths = loadPaths(
                sectWriteEvents, "exclude_paths", true, {});
    cleanExcludePaths(m_wSettings.includePaths, hiddenPaths, m_wSettings.excludePaths,
                         sectWriteEvents->sectionName());

    bool insertMaxEventCount;
    uint32_t maxEventCount;
    if(m_parsedCfgVersion < QVersionNumber{2,4}){
        // Backwards compatibility: old versions did not impose
        // an event limit
        insertMaxEventCount = true;
        maxEventCount = 0;
    } else {
        insertMaxEventCount = false;
        maxEventCount = 5000;
    }
    m_wSettings.maxEventCount = sectWriteEvents->getValue<uint32_t>(
                "max_event_count", maxEventCount, insertMaxEventCount);
    m_wSettings.maxEventCount = (m_wSettings.maxEventCount == 0) ?
                                    numeric_limits<uint64_t>::max() :
                                    m_wSettings.maxEventCount;

}

void Settings::loadSectRead()
{
    auto sectReadEvents  = m_cfg[SECT_READ_NAME];

    sectReadEvents->setComments(qtr(
                                   "Configure, which paths shall be observed for "
                                   "read- or exec-events. Put each desired path into "
                                   "a separate line. "
                                   "Per default read file-events are only logged, "
                                   "if you have *also* write permission (assuming other "
                                   "read files are not of interest)."
                                   ));
    m_rSettings.enable = sectReadEvents->getValue<bool>(SECT_READ_KEY_ENABLE, true);
    m_rSettings.onlyWritable = sectReadEvents->getValue<bool>("only_writable", true);
    m_rSettings.excludeHidden = sectReadEvents->getValue<bool>("exclude_hidden", true);
    PathTree* hiddenPaths = (m_rSettings.excludeHidden) ? m_rSettings.includePathsHidden.get() : nullptr;
    m_rSettings.includePaths = loadPaths(
                sectReadEvents, SECT_READ_KEY_INCLUDE_PATHS, true, {"$HOME"}, hiddenPaths);
    m_rSettings.excludePaths = loadPaths(
                sectReadEvents, "exclude_paths", true, {});
    cleanExcludePaths(m_rSettings.includePaths, hiddenPaths, m_rSettings.excludePaths,
                         sectReadEvents->sectionName());

    bool insertMaxEventCount;
    uint32_t maxEventCount;
    if(m_parsedCfgVersion < QVersionNumber{2,4}){
        // Backwards compatibility: older versions did not impose
        // an event limit
        insertMaxEventCount = true;
        maxEventCount = 0;
    } else {
        insertMaxEventCount = false;
        maxEventCount = 5000;
    }
    m_rSettings.maxEventCount = sectReadEvents->getValue<uint32_t>(
                "max_event_count", maxEventCount, insertMaxEventCount);
    m_rSettings.maxEventCount = (m_rSettings.maxEventCount == 0) ?
                                    numeric_limits<uint64_t>::max() :
                                    m_rSettings.maxEventCount;
}

void Settings::loadSectScriptFiles()
{
    auto sectScriptFiles = m_cfg[SECT_SCRIPTS_NAME];
    const QString scriptFiles_OnlyWritableKey = "only_writable";
    sectScriptFiles->setComments(
                qtr("Configure what files (scripts), which were *read* "
                    "by the observed command, shall be stored within "
                    "%1's database.\n"
                    "The maximal filesize may have units such as KiB, MiB, etc.. "
                    "You can specify file-extensions or mime-types "
                    "(only with the fanotify-backend) to match desired "
                    "file-types, e.g. sh (without leading dot!) or application/x-shellscript. "
                    "The following rules apply: if both are unset, "
                    "accept all file-types (not recommended), if one of the "
                    "two is unset then only the set one is considered, if both "
                    "are set, at least one of the two has to match for the file to "
                    "be stored. "
                    "Note that finding out the mimetype is a lot more "
                    "computationally expensive than the file-extension-method. %1 "
                    "can list a mimetype for a given file, see also %1 --help."
                    "\n"
                    "Per default only the first N read files matching all given rules "
                    "are saved for each command-sequence (max_count_of_files).\n"
                    "%2: only store a read file, if you have write- (not only read-) "
                    "permission for it.\n"
                    "Storing read files is disabled by default.\n"
                    ).arg(app::SHOURNAL, scriptFiles_OnlyWritableKey));
    m_scriptSettings.enable = sectScriptFiles->getValue<bool>(SECT_SCRIPTS_ENABLE, false);
    m_scriptSettings.onlyWritable = sectScriptFiles->getValue<bool>(scriptFiles_OnlyWritableKey, true);
    m_scriptSettings.maxFileSize = sectScriptFiles->getFileSize("max_size", 500*1024) ;
    m_scriptSettings.maxCountOfFiles = static_cast<int>(sectScriptFiles->getValue<uint>(
                "max_count_of_files", 3));
    m_scriptSettings.excludeHidden = sectScriptFiles->getValue<bool>("exclude_hidden", true);
    PathTree* hiddenPaths = (m_scriptSettings.excludeHidden) ?
                m_scriptSettings.includePathsHidden.get() : nullptr;

    m_scriptSettings.includeExtensions = sectScriptFiles->getValues<StrLightSet>(
                SECT_SCRIPTS_INCLUDE_FILE_EXTENSIONS, {"sh"}, false, "\n");
    m_scriptSettings.includeMimetypes = sectScriptFiles->getValues<MimeSet>(
                "include_mime_types", {"application/x-shellscript"}, false, "\n");



    m_scriptSettings.includePaths = loadPaths(sectScriptFiles, SECT_SCRIPTS_INCLUDE_PATHS,
                                              true, {"/"}, hiddenPaths);
    m_scriptSettings.excludePaths = loadPaths(sectScriptFiles, "exclude_paths",
                                                 true, {});
    cleanExcludePaths(m_scriptSettings.includePaths, hiddenPaths, m_scriptSettings.excludePaths,
                         sectScriptFiles->sectionName());

    // make user configurable? If so, make sure not bigger than sizeof(int)/2...
    m_scriptSettings.flushToDiskTotalSize = 1024 * 1024 * 10;
}

void Settings::loadSectIgnoreCmd()
{
    auto sectIgnoreCmd   = m_cfg["Ignore-commands"];
    m_ignoreCmdsRegardlessOfArgs.clear();
    m_ignoreCmds.clear();

    const QString sect_ignore_cmds_commands = "commands";

    sectIgnoreCmd->setComments(qtr(
                      "Only applies to the shell-integration and the\n"
                      "fanotify backend!\n"
                      "Exclude specific commands from observation. "
                      "The (optional) path to the commands must not contain whitepaces "
                      "(create a symlink and import that PATH, if necessary). "
                      "You can provide arguments so that a given "
                      "command is only excluded, if it is followed "
                      "by exactly the given arguments (order matters). "
                      "Further wildcards (*) are supported, but ONLY "
                      "for commands, so that a command can be excluded "
                      "regardless of its arguments.\n\n"
                      "%1 = '''bash\n"
                      "bash -i\n"
                      "screen\n"
                      "mount*'''\n").arg(sect_ignore_cmds_commands)
                              );

    for(const auto & c : defaultIgnoreCmds()){
        addIgnoreCmd(c, false, sectIgnoreCmd->sectionName());
    }

    sectIgnoreCmd->setInsertDefaultToComments(false);
    for(const auto & c : sectIgnoreCmd->getValues<QStringList>(sect_ignore_cmds_commands,
                                                              QStringList(),
                                                              false, "\n")) {
        addIgnoreCmd(c, true, sectIgnoreCmd->sectionName());
    }
    sectIgnoreCmd->setInsertDefaultToComments(true);
}

void Settings::loadSectMount()
{
    auto sectMount       = m_cfg["mounts"];
    const QString sect_mount_ignore = "exclude_paths";

    sectMount->setComments(qtr(
                           "Only applies to the fanotify backend!"
                           "Ignore sub-mount-paths from observation. "
                           "This is typically only needed, if "
                           "you don't have permissions on some "
                           "mounts and want to supress warnings. "
                           "Pseudo-filesytems like /proc are already excluded. "
                           "Put each absolute path into a separate line.\n"
                           "To ignore mounts for which you don't have access permissions, "
                           "set the respective flag to true.\n"
                           )
                          );
    m_mountIgnoreNoPerm = sectMount->getValue<bool>("ignore_no_permission", false);

    m_mountIgnorePaths = sectMount->getValues<StrLightSet>(sect_mount_ignore,
                                                       {},
                                                       false, "\n");

    std::vector<const char*> defaultMountIgnorePaths = {"/proc", "/sys", "/run",
                          "/dev/hugepages", "/dev/mqueue", "/dev/pts"};
    m_mountIgnorePaths.insert(defaultMountIgnorePaths.begin(), defaultMountIgnorePaths.end());
    if(os::getuid() != 0){
        m_mountIgnorePaths.insert("/root");
    }
}

void Settings::loadSectHash()
{
    auto sectHash        = m_cfg["Hash"];

    const QString sect_hash_enable = "enable";
    const QString sect_hash_chunksize = "chunksize";
    const QString sect_hash_maxCountReads = "max-count-reads";

    sectHash->setComments(qtr(
                          "Note: this section includes advanced settings and should not be "
                          "changed at all in most cases and if so, only with a fresh database. "
                          "%1 or %2 should *not* be changed during the lifetime of the database. "
                          "Changing it is not a well tested feature and in any case causes overhead "
                          "for hash-based database-queries.").
                          arg(sect_hash_chunksize, sect_hash_maxCountReads));

    m_hashSettings.hashEnable = sectHash->getValue<bool>(sect_hash_enable, true, true);
    // Exclude negative values by using uint
    m_hashSettings.hashMeta.chunkSize = static_cast<HashMeta::size_type>(
                sectHash->getValue<uint>(sect_hash_chunksize, 256, true));
    m_hashSettings.hashMeta.maxCountOfReads = static_cast<HashMeta::size_type>(
                sectHash->getValue<uint>(sect_hash_maxCountReads, 3, true));
    if(m_hashSettings.hashEnable){
        // TODO: also limit maxCountOfReads -> see kernel module

        if(m_hashSettings.hashMeta.chunkSize < 8 ||
                m_hashSettings.hashMeta.chunkSize > 1024 * 40 ||
                m_hashSettings.hashMeta.maxCountOfReads < 1){
            throw ExcCfg(qtr("Invalid hashsettings. Must be:"
                             " 8 >= %1 <= 40KiB  and %2 > 1")
                         .arg(sect_hash_chunksize, sect_hash_maxCountReads));
        }
    }
}


Settings::ReadVersionReturn Settings::readVersion(SafeFileUpdate& verUpd8)
{
    ReadVersionReturn ret;
    verUpd8.read([&ret, &verUpd8]{
        ret.ver = QVersionNumber::fromString(QTextStream(&verUpd8.file()).readLine());
    });
    ret.verFilePath = verUpd8.file().fileName();
    if(ret.ver.isNull() ){
        // check legacy version file (migrated...)
        ret.ver = readLegacyConfigFileVersion();
        if(ret.ver.isNull()){
            logInfo << qtr("No valid version-file found, although a config-file existed. This "
                           "should only happen during the transition from shournal < 2.1 "
                           "to a version >= 2.1.");
            ret.ver = app::initialVersion();
        } else {
            ret.verFilePath = legacyCfgVersionFilePath();
        }
    }
    return ret;
}

/// If cached cfg-version is newer than our app's version, throw,
/// if it is older, migrate sections to new names.
void Settings::handleUnequalVersions(Settings::ReadVersionReturn &readVerResult)
{
    if(readVerResult.ver > app::version()){
         throw ExcCfg(qtr("The config-file version is greater than the "
                          "application version. This most likely happens "
                          "if running shournal's shell integration while "
                          "shournal was updated. In that case "
                          "simply exit the shell session and start it again. "
                          "Otherwise you might have "
                          "downgraded shournal and need to manually correct "
                          "the version-file at %1. "
                          "Cached version is %2, current application version is %3")
                      .arg(readVerResult.verFilePath)
                      .arg(readVerResult.ver.toString())
                      .arg(app::version().toString()));
    }

    assert( readVerResult.ver < app::version() );
    if(readVerResult.ver < QVersionNumber{0,9}){
        logDebug << "updating cfg-file to" << QVersionNumber{0,9}.toString();
        m_cfg.renameParsedSection("Hash", "Hash for file write-events");
    }
    if(readVerResult.ver < QVersionNumber{2,1}){
        logDebug << "updating cfg-file to" << QVersionNumber{2,1}.toString();
        // Because of new section  [File read-events] for read-events for which
        // no (script-) files shall be stored, rename old [File read-events].
        // Read files now also support hash, so rename the hash-section (again).
        m_cfg.renameParsedSection("File read-events", "File read-events storage settings");
        m_cfg.renameParsedSection("Hash for file write-events", "Hash");
    }
}

/// Store the config to disk. Note that this is only done for new versions,
/// that's why the version file is also updated alongside.
void Settings::storeCfg(SafeFileUpdate &cfgUpd8, SafeFileUpdate &verUpd8)
{
    cfgUpd8.write([this, &cfgUpd8]{
        m_cfg.store(cfgUpd8.file());
    });

    verUpd8.write([&verUpd8, &cfgUpd8]{
        QTextStream(&verUpd8.file()) << app::version().toString();

        QFileInfo legacyVersionInfo(legacyCfgVersionFilePath());
        if(legacyVersionInfo.exists() && ! legacyVersionInfo.isSymLink()){
            // atomically create symlink to new cfg version file (using a temporary
            // symlink and rename/move that):
            logInfo << qtr("handle legacy config version file: creating symlink to "
                           "new location...");
            auto uuid = make_uuid();
            QByteArray cfgPathBytes = cfgUpd8.file().fileName().toLocal8Bit();
            QByteArray tmpSymlinkLocation = legacyVersionInfo.absoluteDir().filePath(uuid).toLocal8Bit();
            os::symlink(cfgPathBytes.constData(), tmpSymlinkLocation.constData());
            os::rename(tmpSymlinkLocation, legacyVersionInfo.absoluteFilePath().toLocal8Bit());
        }
    });

}

/// Parse or create the configuration file at the system's config path
/// (please perform QCoreApplication::setApplicationName() before).
/// Another file at config dir provides the version. If the config file version is greater
/// than app-version, throw, if smaller, update the version and possibly
/// the config-file-scheme as well. Scheme updates
/// for sections work by directly renaming the sections in loadSections() and
/// by moving the old to new sections in handleUnequalVersions(). Note that
/// this also works in case of "redundant" scheme updates, where over multiple
/// scheme versions the same section is renamed multiple times. Intermediate
/// sections are created as necessary and potentially dropped/renamed again
/// by subsequent scheme updates.
/// @throws ExcCfg
void Settings::load()
{
    const auto cfgPath = cfgFilepath();

    const QString cfgDir(splitAbsPath(cfgPath).first);
    QDir dir;
    if(! dir.mkpath(cfgDir) ){
        throw QExcIo(qtr("Failed to create configuration directory at %1")
                                 .arg(cfgDir) );
    }

    SafeFileUpdate cfgUpd8(cfgPath);
    bool cfgFileExisted = cfgUpd8.read([this, &cfgUpd8]{
        m_cfg.parse(cfgUpd8.file());
    });

    SafeFileUpdate verUpd8(pathJoinFilename(cfgDir, QString(".config-version")));
    bool cfgVersionNeedsUpdate = false;
    m_parsedCfgVersion = app::version() ;
    if(cfgFileExisted){
        // do we need a version update?
        auto readVerRet = readVersion(verUpd8);
        if(readVerRet.ver != app::version()){
            m_parsedCfgVersion = readVerRet.ver;
            cfgVersionNeedsUpdate = true;
            handleUnequalVersions(readVerRet);
        }
    }
    try {
        loadSections();

        auto notReadKeys = m_cfg.generateNonReadSectionKeyPairs();
        if(! notReadKeys.isEmpty()){
            throw ExcCfg(qtr("Unexpected key in section [%1] - '%2'")
                          .arg(notReadKeys.first().first,
                               *notReadKeys.first().second.begin()));
        }
        // Only write configuration to disk, if there was no such file
        // or we are running a new version for the first time
        if(! cfgFileExisted || cfgVersionNeedsUpdate){
            logDebug << "about to update config at" << cfgPath;
            storeCfg(cfgUpd8, verUpd8);
        }
    } catch(ExcCfg & ex) {
        ex.setDescrip(ex.descrip() + qtr(". The config file resides at %1").arg(cfgPath));
        throw;
    }

    m_settingsLoaded = true;
}

/// Select which backend to choose based on local/global
/// config files and availability (search $PATH).
/// @return app::SHOURNAL_RUN, app::SHOURNAL_RUN_FANOTIFY or an
/// empty string if not found.
QString Settings::chooseShournalRunBackend()
{
    auto appname = QCoreApplication::applicationName();
    const QString localPath(cfgAppDir() + "/backend");
    const QString globalPath = "/etc/shournal.d/backend";
    QString selectedPath;

    if(QFile::exists(localPath)){
        selectedPath = localPath;
    } else if(QFile::exists(globalPath)){
        selectedPath = globalPath;
    }

    if(! selectedPath.isEmpty()){
        // load backend from file
        QFileThrow backendCfgFile(selectedPath);
        backendCfgFile.open(QFile::OpenModeFlag::ReadOnly);
        QTextStream s(&backendCfgFile);
        auto backendStr =  s.readLine();
        if(backendStr == "ko") {
            return app::SHOURNAL_RUN;
        }
        if(backendStr == "fanotify") {
            return app::SHOURNAL_RUN_FANOTIFY;
        }
        logWarning << qtr("Invalid backend %1 at file %2 - "
                          "supported options: [fanotify, ko]. "
                          "Using defaults...").arg(backendStr, selectedPath);
    }

   if( ! QStandardPaths::findExecutable(app::SHOURNAL_RUN).isEmpty())
       return app::SHOURNAL_RUN;

   if( ! QStandardPaths::findExecutable(app::SHOURNAL_RUN_FANOTIFY).isEmpty())
       return app::SHOURNAL_RUN_FANOTIFY;

   return {};
}


const Settings::StrLightSet &Settings::getMountIgnorePaths()
{
    assert(m_settingsLoaded);
    return m_mountIgnorePaths;
}

bool Settings::getMountIgnoreNoPerm() const
{
    return m_mountIgnoreNoPerm;
}



const Settings::StringSet &Settings::ignoreCmds()
{
    return m_ignoreCmds;
}

const Settings::StringSet &Settings::ignoreCmdsRegardslessOfArgs()
{
    return m_ignoreCmdsRegardlessOfArgs;
}


const Settings::WriteFileSettings &Settings::writeFileSettings() const
{
    return m_wSettings;
}

const Settings::ReadFileSettings &Settings::readFileSettings() const
{
    return m_rSettings;
}

const Settings::ScriptFileSettings &Settings::readEventScriptSettings() const
{
    return m_scriptSettings;
}

const Settings::HashSettings &Settings::hashSettings() const
{
    return m_hashSettings;
}








