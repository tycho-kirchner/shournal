#pragma once

/// Abstract base class for shells
class AttachedShell
{
public:
    AttachedShell();
    virtual ~AttachedShell();

    virtual void handleEnable();
    virtual bool lastCmdWasValid();

};
