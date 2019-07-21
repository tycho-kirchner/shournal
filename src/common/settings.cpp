
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
#include "user_str_conversions.h"

using qsimplecfg::Section;
using qsimplecfg::ExcCfg;
using StringSet = Settings::StringSet;


Settings &Settings::instance()
{
    static Settings s;
    return s;
}


Settings::Settings() :
    m_mountIgnoreNoPerm(false),
    m_settingsLoaded(false)
{}


Settings::ReadFileSettings::ReadFileSettings() :
    enable(false),
    onlyWritable(true),
    maxFileSize(500*1024),
    maxCountOfFiles(3),
    flushToDiskTotalSize(1024*1024*10)
{}


Settings::WriteFileSettings::WriteFileSettings() :
    hashEnable(true),
    onlyClosedWrite(true),
    flushToDiskEventCount(2000)
{}




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


QString Settings::defaultCfgFilepath()
{
    QString p(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
              + '/' + QCoreApplication::applicationName() + '/' + "config.ini");
    return p;
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




static PathTree loadPaths(Section& section, const QString& keyName,
                          bool eraseSubpaths,
                                    const std::unordered_set<QString> & defaultValues){
    auto rawPaths = section.getValues<std::unordered_set<QString> >(keyName,
                                              defaultValues,
                                              false, false, "\n");

    PathTree tree;
    for(const auto& p : rawPaths){
        const QString canonicalPath = QDir(p).canonicalPath();
        if(canonicalPath.isEmpty()){
            logWarning << qtr("section %1: %2: path does not exist: %3")
                          .arg(section.sectionName(), keyName, p);
            continue;
        }
        auto pStr = canonicalPath.toStdString();
        // avoid adding needless parent/subpaths

        if(eraseSubpaths){
            if(tree.isSubPath(pStr)){
                logDebug<< keyName << "ignore" << canonicalPath
                         << "because it is a subpath";
                continue;

            }
            // the new path might be a parent path:
            // erase its children (if any)
            auto subPathIt = tree.subpathIter(pStr);
            while(subPathIt != tree.end() ){
                logDebug << keyName << "ignore" << *subPathIt
                         << "because it is a subpath";
                subPathIt = tree.erase(subPathIt);
            }
        }
        tree.insert(pStr);
    }
    return tree;
}

static QString cachedConfigFileVersionFilePath(){
    // don't make it static -> mutliple test cases...
    const QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + "/config-file-version";
    return path;
}


/// Read last cached version from disk. In case of no or an invalid version, return QVersionNumer.isNull==true.
static QVersionNumber readCachedConfigFileVersion(){
    QFile f(cachedConfigFileVersionFilePath());
    if(! f.exists()){
        return {};
    }
    f.open(QFile::OpenModeFlag::ReadOnly);
    CFlock l(f.handle());
    l.lockShared();
    auto ver = QVersionNumber::fromString(QTextStream(&f).readLine());
    if(ver.isNull()){
        logWarning << QString("Bad version string in file %1. Deleting it...")
                      .arg(cachedConfigFileVersionFilePath());
        f.remove();
    }
    return ver;
}


static void writeConfigFileVersionToCache(){
    QFileThrow f(cachedConfigFileVersionFilePath());
    f.open(QFile::OpenModeFlag::WriteOnly);
    CFlock l(f.handle());
    l.lockExclusive();
    QTextStream stream(&f);
    stream << app::version().toString();
}


static void validateExcludePaths(const PathTree& includePaths, PathTree& excludePaths,
                                 const QString& sectionName){
    for(auto it=excludePaths.begin(); it != excludePaths.end();){
        if(! includePaths.isSubPath(*it)){
            logWarning << qtr("section %1: ignore exclude-path %2 - it is not a sub-path "
                           "of any include-path").arg(sectionName).arg((*it).c_str());
            it = excludePaths.erase(it);
        } else {
            ++it;
        }
    }
}


/// Parse or create the configuration file at path or
/// use the default one (please QCoreApplication::setApplicationName()
/// before)
/// @throws ExcCfg
void Settings::load(const QString &passedPath)
{
    QString path;
    if(! passedPath.isEmpty()){
        path = passedPath;
    } else {
        path = defaultCfgFilepath();
    }

    bool cfgFileExisted = QFileInfo::exists(path);

    m_cfg.parse(path);
    UserStrConversions userStrConv;

    try {
        m_cfg.setInitialComments(qtr(
                                 "Configuration file for %1. Uncomment lines "
                                 "to change defaults. Multi-line-values (e.g. paths) "
                                 "are framed by leading and trailing "
                                 "triple-quotes ''' .").arg(app::SHOURNAL)
                                 );

        // ------------------------------------------------------------------
        const QString sectWriteEvents_sectName = "File write-events";
        Section & sectWriteEvents = m_cfg[sectWriteEvents_sectName];
        sectWriteEvents.setComments(qtr(
                              "Configure, which paths shall be observed for "
                              "*write*-events. Put each desired path into "
                              "a separate line. "
                              "Default is to observe all paths.\n"
                              ));
        m_wSettings.includePaths = loadPaths(
                    sectWriteEvents, "include_paths", true, {"/"});
        m_wSettings.excludePaths = loadPaths(
                    sectWriteEvents, "exclude_paths", true, {});
        validateExcludePaths(m_wSettings.includePaths, m_wSettings.excludePaths,
                             sectWriteEvents_sectName);



        // -----------------------------------------------------------------
        const QString readFilesOnlyWritableKey = "only_writable";
        const QString sectReadFiles_sectName = "File read-events";
        Section & sectReadFiles  = m_cfg[sectReadFiles_sectName];
        sectReadFiles.setComments(
                    qtr("Configure what files (scripts), which were *read* "
                        "by the observed command, shall be stored within "
                        "%1's database.\n"
                        "The maximal filesize may have units such as KiB, MiB, etc.. "
                        "Don't specify huge values here, because, at least for a short "
                        "time, the file is stored in RAM.\n"
                        "You can specify file-extensions or mime-types to match desired "
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
                        "%2: only store read files, if you have write- (not only read-) "
                        "permission for it.\n"
                        "Storing read files is disabled by default.\n"
                                    ).arg(app::SHOURNAL, readFilesOnlyWritableKey));
        m_rSettings.enable = sectReadFiles.getValue<bool>("enable", false);
        m_rSettings.onlyWritable = sectReadFiles.getValue<bool>(readFilesOnlyWritableKey, true);
        m_rSettings.maxFileSize = sectReadFiles.getFileSize("max_size", 500*1024) ;
        m_rSettings.includeExtensions = sectReadFiles.getValues<StringSet>(
                    "include_file_extensions", {"sh"}, false, false, "\n");
        m_rSettings.includeMimetypes = sectReadFiles.getValues<MimeSet>(
                    "include_mime_types", {"application/x-shellscript"}, false, false, "\n");
        m_rSettings.maxCountOfFiles = static_cast<int>(sectReadFiles.getValue<uint>(
                    "max_count_of_files", 3));

        // make user configurable? If so, make sure not bigger than sizeof(int)/2...
        m_rSettings.flushToDiskTotalSize = 1024 * 1024 * 10;

        m_rSettings.includePaths = loadPaths(sectReadFiles, "include_paths",
                                                  true, {"/"});
        m_rSettings.excludePaths = loadPaths(sectReadFiles, "exclude_paths",
                                                     true, {});
        validateExcludePaths(m_rSettings.includePaths, m_rSettings.excludePaths,
                             sectReadFiles_sectName);

        // ------------------------------------------------------------------
        m_ignoreCmdsRegardlessOfArgs.clear();
        m_ignoreCmds.clear();

        const QString sect_ignore_cmds_sectName = "Ignore-commands";
        const QString sect_ignore_cmds_commands = "commands";

        Section & sectIgnoreCmd =m_cfg[sect_ignore_cmds_sectName];
        sectIgnoreCmd.setComments(qtr(
                          "Only applies to the shell-integration.\n"
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
            addIgnoreCmd(c, false, sect_ignore_cmds_sectName);
        }

        sectIgnoreCmd.setInsertDefaultToComments(false);
        for(const auto & c : sectIgnoreCmd.getValues<QStringList>(sect_ignore_cmds_commands,
                                                                  QStringList(),
                                                                  false, false, "\n")) {
            addIgnoreCmd(c, true, sect_ignore_cmds_sectName);
        }

        // for(const auto & p : g_ignoreCmds){
        //     logDebug << "PATH" << p;
        // }
        //
        // for(const auto & p : g_ignoreCmdsRegardlessOfArgs){
        //     logDebug << "PATH_REGARDLESS" << p;
        // }


        // ------------------------------------------------------------------
        const QString sect_mount_sectName = "mounts";
        const QString sect_mount_ignore = "exclude_paths";

        Section & sectMount = m_cfg[sect_mount_sectName];
        sectMount.setComments(qtr(
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

        m_mountIgnorePaths = sectMount.getValues<StringSet>(sect_mount_ignore,
                                                           {},
                                                           false, false, "\n");

        std::vector<const char*> defaultMountIgnorePaths = {"/proc", "/sys", "/run",
                              "/dev/hugepages", "/dev/mqueue", "/dev/pts"};
        m_mountIgnorePaths.insert(defaultMountIgnorePaths.begin(), defaultMountIgnorePaths.end());
        if(os::getuid() != 0){
            m_mountIgnorePaths.insert("/root");
        }

        m_mountIgnoreNoPerm = sectMount.getValue<bool>("ignore_no_permission", false);


        // ------------------------------------------------------------------
        const QString sect_hash_sectName = "Hash for file write-events";
        const QString sect_hash_enable = "enable";
        const QString sect_hash_chunksize = "chunksize";
        const QString sect_hash_maxCountReads = "max-count-reads";

        Section & sectHash = m_cfg[sect_hash_sectName];
        sectHash.setComments(qtr(
                              "Note: this section includes advanced settings and should not be "
                              "changed in most cases. "
                              "Changing %1 or %2 during the lifetime of the database "
                              "might yield the necessity "
                              "to hash (different parts of) "
                              "the same file multiple times for file-queries.").
                             arg(sect_hash_chunksize, sect_hash_maxCountReads));

        m_wSettings.hashEnable = sectHash.getValue<bool>(sect_hash_enable, true, true);
        // Exclude negative values by using uint
        m_wSettings.hashMeta.chunkSize = static_cast<HashMeta::size_type>(
                    sectHash.getValue<uint>(sect_hash_chunksize, 4096, true));
        m_wSettings.hashMeta.maxCountOfReads = static_cast<HashMeta::size_type>(
                    sectHash.getValue<uint>(sect_hash_maxCountReads, 20, true));

        // maybe_todo: also record write events (not only closewrite)
        // - make it configurable -> test it...
        m_wSettings.onlyClosedWrite = true;

        // ------------------------------------------------------------------

        // Only write configuration to disk, if there is no such file
        // or we are running a new version for the first time
        auto cachedVersion = readCachedConfigFileVersion();
        int cmpResult = QVersionNumber::compare(cachedVersion, app::version());
        if(cmpResult >= 1){
             throw ExcCfg(qtr("The config-file version is greater than the application version. This most likely happens "
                              "if running shournal's shell integration while shournal was updated. In that case "
                              "simply exit the shell session and start it again. Otherwise you might have "
                              "downgraded shournal and need to manually correct the cached version at %1. "
                              "Cached version is %2, current application version is %3")
                          .arg(cachedConfigFileVersionFilePath())
                          .arg(cachedVersion.toString())
                          .arg(app::version().toString()));
        }


        if(! cfgFileExisted ||
               cmpResult <= -1 ){
            if(QVersionNumber::compare(cachedVersion, {0,9}) <= -1 ){
                m_cfg.moveValsToNewSect("Hash", "Hash for file write-events");
            }

            m_cfg.store();
            writeConfigFileVersionToCache();
        }


        auto notReadKeys = m_cfg.generateNonReadSectionKeyPairs();
        if(! notReadKeys.isEmpty()){
            throw ExcCfg(qtr("Unexpected key in section [%1] - '%2'")
                          .arg(notReadKeys.first().first,
                               *notReadKeys.first().second.begin()));
        }

    } catch(ExcCfg & ex){
        ex.setDescrip(ex.descrip() + qtr(". The config file resides at %1").arg(path));
        throw;
    }

    m_settingsLoaded = true;
}




const Settings::StringSet &Settings::getMountIgnorePaths()
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

const QString &Settings::lastLoadedFilepath()
{
    return m_cfg.lastFilePath();
}



const Settings::ReadFileSettings &Settings::readEventSettings()
{
    return m_rSettings;
}

const Settings::WriteFileSettings &Settings::writeFileSettings()
{
    return m_wSettings;
}







