#pragma once

#include "util.h"

/// Abstract base class for shells
class AttachedShell
{
public:
    AttachedShell() = default;
    virtual ~AttachedShell() = default;

    virtual void handleEnable();
    virtual bool cmdCounterJustIncremented();

public:
    Q_DISABLE_COPY(AttachedShell)
    DEFAULT_MOVE(AttachedShell)
};
