#pragma once

#include <stdio.h>

#include "exccommon.h"
#include "settings.h"


struct shournalk_group;

class ExcShournalk : public QExcCommon
{
public:
    ExcShournalk(const QString & text);
};

/// c++ interface for shournal's kernel module.
class ShournalkControl {
public:
    ShournalkControl();
    ~ShournalkControl();

    void doMark(pid_t pid);
    void preparePollOnce();

    void removePid(pid_t pid);

    FILE *tmpFileTarget() const;
    shournalk_group *kgrp() const;

private:
    Q_DISABLE_COPY(ShournalkControl)
    struct shournalk_group* m_kgrp;
    FILE* m_tmpFileTarget;

    void markPaths(const Settings::StrLightSet& paths, int path_tpye);
    void markExtensions(const Settings::StrLightSet& extensions, int ext_type);
    void doMarkExtensions(const StrLight &extensions, int ext_type);
};
