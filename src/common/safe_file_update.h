#pragma once

#include <sys/stat.h>
#include <unistd.h>
#include <iostream>

#include "interrupt_handler.h"
#include "logger.h"
#include "os.h"
#include "osutil.h"
#include "qfilethrow.h"
#include "util.h"

/// Safeley read and update (config) files without locking, even on NFS(!).
/// For 'normal' filesystems (e.g. ext4) renaming files within the same filesystem
/// is an 'atomic' operation. However, on NFS this is not the case (see e.g.
/// https://serverfault.com/questions/817887/rename-on-nfs-atomicity ). However, link-
/// or directory-creation is referred being atomic (https://unix.stackexchange.com/a/125946).
/// Therefore, we use the following procedure:
/// The basic procedure is:
/// * Updaters 'atomically' create a lock_dir, do the rename, sync, and remove the lock
/// * Readers are prepared for non-existing files and stale reads, whereupon they try to
///   gain the lock themselves. Once they got the lock, they try to read the config file
///   again, to differentiate file-in-update from file-not-exist.
class SafeFileUpdate
{
public:
    explicit SafeFileUpdate(const QString& filepath):
        m_filepath(filepath),
        m_lockfilepath(filepath.toLocal8Bit() + "__lock"),
        m_file(filepath)
    {}

    ~SafeFileUpdate() {
        // Do not throw from destructor
        try {
            if(m_isLocked){
                doUnlock();
            }
        } catch (const std::exception& ex ) {
            std::cerr << ex.what() << "\n";
        }
    }

    QFileThrow& file() {
        return m_file;
    }

    template <typename F>
    bool read(F func){
        const QIODevice::OpenMode openflags = QIODevice::OpenModeFlag::ReadOnly |
                QIODevice::OpenModeFlag::Text;
        auto finallyClose = finally([this]{ if(m_file.isOpen()){ m_file.close(); } });
        try {
            m_file.open(openflags);
            func();
            return true;
        } catch (const QExcIo& ex) {
            switch (ex.errorNumber()) {
            case ENOENT: break;
            case ESTALE: break;
            default:
                logWarning << "unhandled error in" <<__func__ << "file" << m_filepath;
                throw;
            }
        }
        doLock();
        // logDebug << "got lock for reading";
        QFileInfo fileInfo(m_filepath);
        if(! fileInfo.exists()){
            // We got the lock, but the file still does not exist.
            // In the case of config files this usually means, that we'll
            // initially create it later. We keep the lock.
            logDebug << "file does not exist:" << m_filepath;
            return false;
        }
        // We got the lock and the file exists. Reading it should now succeed
        try {
            // reopen the file anyway, as the descriptor might be 'stale'.
            if(m_file.isOpen()){
                 m_file.close();
             }
             m_file.open(openflags);
            func();
        } catch (const std::exception&) {
            logDebug << "second attempt failed altough we got the lock. Oh oh...";
            doUnlock();
            throw;
        }
        return true;
    }

    template <typename F>
    void write(F func){
        bool renameSuccess = false;
        int fd = -1;
        if(! m_isLocked){
            doLock();
        }
        // logDebug << "got lock for update";

        QByteArray tmpFilepath = pathJoinFilename(
                    QFileInfo(m_filepath).absolutePath().toLocal8Bit(),
                    QByteArray("tmp.XXXXXX"));
        auto finalActions = finally([this, &fd, &renameSuccess, &tmpFilepath]{
            if(fd != -1 && ! renameSuccess){
                os::remove(tmpFilepath);
            }
            if(m_file.isOpen()){
                m_file.close();
            }
            this->doUnlock();
        });

        fd = osutil::mktmp(tmpFilepath);
        m_file.open(fd, QIODevice::OpenModeFlag::ReadWrite, QFileDevice::AutoCloseHandle);
        func();
        // Also flushes the file before sync
        m_file.close();
        auto dstPath = m_filepath.toLocal8Bit();
        os::rename(tmpFilepath, dstPath);
        renameSuccess = true;
        sync();
    }

public:
    Q_DISABLE_COPY(SafeFileUpdate)

private:
    QString m_filepath;
    QByteArray m_lockfilepath;
    QFileThrow m_file;
    bool m_isLocked{false};
    InterruptProtect m_interruptProtect;

    void doLock(){
        if(m_isLocked){
            throw QExcProgramming(QString(__func__) + ": already locked " + m_filepath);
        }
        QFileInfo fileInfo(m_filepath);
        if(! QDir().mkpath(fileInfo.absolutePath())){
            throw QExcIo(qtr("Failed to create directories for path %1")
                                     .arg(fileInfo.absolutePath()) );
        }

        m_interruptProtect.enable(os::catchableTermSignals());

        // try to lock the path by creating a dir. Link creation
        // is atomically on NFS.
        for(int i=0; i < 10; i++){
            if(mkdir(m_lockfilepath.data(), 0755) == 0){
                m_isLocked = true;
                return;
            }
            if(errno != EEXIST){
                throw QExcIo("failed to create lockfile " + m_lockfilepath);
            }
            sleep(1);
        }
        throw QExcIo("Gave up creating lock-directory " + m_lockfilepath +
                     ". If it's not a load-problem, please remove the stale directory.");
    }

    void doUnlock(){
        if(!m_isLocked){
            throw QExcProgramming(QString(__func__) + ": not locked " + m_filepath);

        }
        if(rmdir(m_lockfilepath) != 0){
            logCritical << __func__ << "failed to remove lockpath:" << strerror(errno);
        }
        m_interruptProtect.disable();
        m_isLocked = false;
    }

};
