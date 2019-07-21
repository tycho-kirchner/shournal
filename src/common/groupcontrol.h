#pragma once

#include <vector>

#include "idmapentry.h"
#include "os.h"

namespace groupcontrol {
typedef std::vector<S_IdMapEntry<gid_t> > GidMapRanges_T;

os::Groups generateRealGroups();

GidMapRanges_T
    generateGidMapRanges(const os::Groups & groups);


}
