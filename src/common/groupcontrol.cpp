
#include <algorithm>
#include <QDebug>

#include "groupcontrol.h"
#include "os.h"


/// @return all 'real' groups, the calling user is a member
/// of
os::Groups
    groupcontrol::generateRealGroups(){
    // according to man getgroups(2) "it is unspecified whether the
    // effective group ID of the calling process is included in the returned
    // list".
    os::Groups groups = os::getgroups();
    gid_t egid = os::getegid();
    if(egid != os::getgid()){
        auto egidIter = std::find(groups.begin(), groups.end(), egid);
        if(egidIter != groups.end()){
            groups.erase(egidIter);
        }
    }
    return groups;
}

/// create a one-to-one-mapping of param groups for the /proc/$pid/gid_map.
/// Be a little smart and merge consecutive groups into one IdMapEntry.
groupcontrol::GidMapRanges_T groupcontrol::generateGidMapRanges(const os::Groups &groups_){
    if(groups_.empty()){
        qDebug() << "generateGidMapRanges called with empty groups";
        return std::vector<S_IdMapEntry<gid_t>>();
    }

    auto groups = groups_;
    std::sort(groups.begin(), groups.end());

    std::vector<S_IdMapEntry<gid_t>> mapRanges;
    mapRanges.reserve(groups.size());
    mapRanges.emplace_back(groups.at(0));  // at least one group exists (real gid)

    for(size_t i=1; i < groups.size(); i++){
        S_IdMapEntry<gid_t> & previousRange = mapRanges.back();
        if(groups[i] == previousRange.idInNs + 1){
            // gid-range possible
            previousRange.count++;
        } else {
            mapRanges.emplace_back(groups[i]);
        }
    }

    return mapRanges;
}



/////////////////////// PRIVATE /////////////////////////



