#pragma once

#include <string>
#include <unordered_set>
#include <memory>


#include "exccommon.h"
#include "settings.h"

class ExcMountCtrl : public QExcCommon
{
public:
    using QExcCommon::QExcCommon;
};


namespace mountController {
    PathTree generatelMountTree();

}


