#include "mark_helper.h"

#include <sys/user.h>


#include "app.h"
#include "shournalk_ctrl.h"
#include "stdiocpp.h"
#include "translation.h"
#include "os.h"


ExcShournalk::ExcShournalk(const QString &text) :
    QExcCommon(text, false)
{}


using StrLightSet = Settings::StrLightSet;


/// Build the kernel settings according to our own
static shounalk_settings buildKSettings(){
    auto & s = Settings::instance();
    auto & w_sets = s.writeFileSettings();
    auto & r_sets = s.readFileSettings();
    auto & script_sets = s.readEventScriptSettings();

    struct shounalk_settings ksettings{};
    ksettings.w_exclude_hidden = w_sets.excludeHidden;
    ksettings.w_max_event_count = w_sets.maxEventCount;
    ksettings.r_only_writable = r_sets.onlyWritable;
    ksettings.r_exclude_hidden = r_sets.excludeHidden;
    ksettings.r_max_event_count = r_sets.maxEventCount;
    ksettings.r_store_only_writable = script_sets.onlyWritable;
    ksettings.r_store_max_size = unsigned(script_sets.maxFileSize);
    ksettings.r_store_max_count_of_files = uint16_t(script_sets.maxCountOfFiles);
    ksettings.r_store_exclude_hidden = script_sets.excludeHidden;

    if(s.hashSettings().hashEnable){
        ksettings.hash_max_count_reads = s.hashSettings().hashMeta.maxCountOfReads;
        ksettings.hash_chunksize = s.hashSettings().hashMeta.chunkSize;
    }
    return ksettings;
}



ShournalkControl::ShournalkControl()
{
    m_kgrp = shournalk_init(O_CLOEXEC);
    if(m_kgrp == nullptr){
        throw ExcShournalk("init failed");
    }
    shournalk_version kversion;
    if(shournalk_read_version(&kversion) != 0){
        throw ExcShournalk(qtr("Failed to read version from file %1 - %2")
                           .arg(shournalk_versionpath())
                           .arg(translation::strerror_l(errno)));
    }
    if(strcmp(SHOURNAL_VERSION, kversion.ver_str) != 0){
        throw ExcShournalk(qtr("Version mismatch - %1 version is %2, but "
                               "%3 version is %4")
                           .arg(app::SHOURNAL_RUN).arg(SHOURNAL_VERSION)
                           .arg("kernel-module").arg(kversion.ver_str)
                           );
    }

    m_tmpFileTarget = stdiocpp::tmpfile(O_NOATIME); // tmpfile auto deletes..
    if(m_tmpFileTarget == nullptr){
        throw ExcShournalk(qtr("Failed to open temporary event target-file: %1")
                           .arg(translation::strerror_l(errno)));
    }
    int fd = fileno_unlocked(m_tmpFileTarget);
    shournalk_set_target_fd(m_kgrp, fd);
}

ShournalkControl::~ShournalkControl()
{
    shournalk_release(m_kgrp);
    fclose(m_tmpFileTarget);
}


/// @throws ExcShournalk
void ShournalkControl::doMark(pid_t pid)
{
    try {
        auto ksettings = buildKSettings();
        shournalk_set_settings(m_kgrp, &ksettings);

        int ret;
        if((ret = shournalk_filter_pid(m_kgrp, SHOURNALK_MARK_ADD, pid)) != 0){
            throw ExcShournalk(translation::strerror_l(ret));
        }
        auto & s = Settings::instance();

        const auto & all_excl = s.getMountIgnorePaths();

        const auto & w_incl = s.writeFileSettings().includePaths->allPaths();
        const auto & w_excl = s.writeFileSettings().excludePaths->allPaths();
        const auto & r_incl = s.readFileSettings().includePaths->allPaths();
        const auto & r_excl = s.readFileSettings().excludePaths->allPaths();
        const auto & script_incl = s.readEventScriptSettings().includePaths->allPaths();
        const auto & script_excl = s.readEventScriptSettings().excludePaths->allPaths();


        markPaths(w_incl, SHOURNALK_MARK_W_INCL );
        markPaths(w_excl, SHOURNALK_MARK_W_EXCL );
        markPaths(all_excl, SHOURNALK_MARK_W_EXCL );

        if(s.readFileSettings().enable){
            markPaths(r_incl, SHOURNALK_MARK_R_INCL);
            markPaths(r_excl, SHOURNALK_MARK_R_EXCL);
            markPaths(all_excl, SHOURNALK_MARK_R_EXCL);
        }
        if(s.readEventScriptSettings().enable){
            markPaths(script_incl, SHOURNALK_MARK_SCRIPT_INCL);
            markPaths(script_excl, SHOURNALK_MARK_SCRIPT_EXCL);
            markPaths(all_excl, SHOURNALK_MARK_SCRIPT_EXCL);
            const auto & exts = s.readEventScriptSettings().includeExtensions;
            if(exts.size()){
                markExtensions(exts, SHOURNALK_MARK_SCRIPT_EXTS);
            }
        }

        if((ret = shournalk_commit(m_kgrp)) != 0){
            throw ExcShournalk(qtr("failed to commit event target - %1")
                               .arg(translation::strerror_l(ret)));
        }
    } catch (ExcShournalk& ex) {
        throw ExcShournalk(qtr("Failed to mark target process with pid "
                           "%1 for observation - %2")
                       .arg(pid).arg(ex.descrip()));
    }
}

void ShournalkControl::preparePollOnce()
{
    if(shournalk_prepare_poll_ONCE(m_kgrp)){
        throw ExcShournalk(qtr("failed to prepare poll"));
    }
}

void ShournalkControl::removePid(pid_t pid)
{
    int ret;
    if((ret = shournalk_filter_pid(m_kgrp, SHOURNALK_MARK_REMOVE, pid)) != 0){
        throw ExcShournalk(
                    qtr("Failed to unmark pid for observation: %1").arg(translation::strerror_l(ret))
                    );
    }
}


FILE *ShournalkControl::tmpFileTarget() const
{
    return m_tmpFileTarget;
}

shournalk_group *ShournalkControl::kgrp() const
{
    return m_kgrp;
}



void ShournalkControl::markPaths(const Settings::StrLightSet& paths, int path_tpye){
    int ret;
    for(const auto& p : paths){
        if((ret = shournalk_filter_string(m_kgrp,
                                        SHOURNALK_MARK_ADD,
                                        path_tpye,
                                        p.c_str())) != 0 ){
            throw ExcShournalk(qtr("failed to mark path "
                                   "%1 - %2").arg(p.c_str())
                                             .arg(translation::strerror_l(ret)));
        }
    }
}


void ShournalkControl::markExtensions
(const Settings::StrLightSet& extensions, int ext_type){
    StrLight extBuf;
    extBuf.reserve(PAGE_SIZE);
    for(const auto & str : extensions){
        // add extensions to a single long string
        // separated by slash. Flush, if bigger
        // than PAGE_SIZE (unlikely)
        if(extBuf.size() + str.size() + 1 > PAGE_SIZE){
            doMarkExtensions(extBuf, ext_type);
        }
        extBuf += str + '/';
    }
    if(! extBuf.empty()){
        doMarkExtensions(extBuf, ext_type);
    }
}


void ShournalkControl::doMarkExtensions
(const StrLight& extensions, int ext_type){
    int ret;
    if((ret = shournalk_filter_string(m_kgrp,
                                    SHOURNALK_MARK_ADD,
                                    ext_type,
                                    extensions.c_str())) != 0 ){
        throw ExcShournalk(qtr("failed to mark extensions - %1 "
                               "- extensions-string: %2")
                           .arg(translation::strerror_l(ret))
                           .arg(extensions.c_str()));
    }
}


