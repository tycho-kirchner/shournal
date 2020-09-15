#include <sys/mount.h>
#include <mntent.h>
#include <unistd.h>
#include <deque>
#include <cstring>
#include <iostream>
#include <sys/stat.h>
#include <regex>

#include <QDebug>
#include <QStandardPaths>

#include "mount_controller.h"
#include "util.h"
#include "settings.h"
#include "excos.h"
#include "os.h"
#include "pathtree.h"
#include "logger.h"
#include "cleanupresource.h"


/// Return all mountpaths from /proc/self/mounts except the
/// ones marked to be ignored (settings).
std::shared_ptr<PathTree> mountController::generatelMountTree(){
    auto & ignoreMountPaths = Settings::instance().getMountIgnorePaths();
    PathTree ignoreMountTree;
    for(const auto & path : ignoreMountPaths){
        ignoreMountTree.insert(path);
    }

    // iterate over all of our mounts
    FILE* mounts = setmntent ("/proc/self/mounts", "r");
    if (mounts == nullptr) {
        throw ExcMountCtrl("setmntent /proc/self/mounts failed");
    }
    auto closeLater = finally([&mounts] { endmntent (mounts); });

    // Determine which submounts shall be ignored and
    // collect the others
    auto mountTree = std::make_shared<PathTree>();
    struct mntent* mnt_;
    while ((mnt_ = getmntent (mounts)) != nullptr) {
        const StrLight mntDir(mnt_->mnt_dir);
        if(ignoreMountTree.isSubPath(mntDir, true)){
            logDebug << "ignoring mountpath" << mntDir.c_str();
            continue;
        }
        mountTree->insert(mntDir);
    }
    return mountTree;
}






