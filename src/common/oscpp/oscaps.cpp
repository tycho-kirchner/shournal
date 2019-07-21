#include <sys/prctl.h>
#include <sys/capability.h>
#include <iostream>

#include "oscaps.h"
#include "excos.h"



static void cap_set_flag_wrapper(cap_t caps,
                          cap_flag_t typeOfFlag,
                          const os::Capabilites::CapFlags &flags,
                          cap_flag_value_t setOrClear){
    if (cap_set_flag(caps, typeOfFlag,
                     int(flags.size()), flags.data(), setOrClear) == -1) {
        throw os::ExcOs(__func__);
    }
}


os::Capabilites::Capabilites(cap_t caps)
    : m_caps(caps)
{}

void os::Capabilites::flush()
{
    // in future maybe something else but only flush to proc, which
    // should be set in constructor (enum)
    this->flushToProc();
}

os::Capabilites::~Capabilites()
{
    if(m_caps != nullptr){
        if(::cap_free(m_caps) == -1){
            perror(__func__);
        }
    }
}

void os::Capabilites::setFlags(cap_flag_t typeOfFlag, const CapFlags &flags,
                               bool autoflush)
{
    cap_set_flag_wrapper(m_caps, typeOfFlag, flags, CAP_SET);
    if(autoflush){
        this->flush();
    }
}

void os::Capabilites::clearFlags(cap_flag_t typeOfFlag, const CapFlags &flags,
                                 bool autoflush)
{
    cap_set_flag_wrapper(m_caps, typeOfFlag, flags, CAP_CLEAR);
    if(autoflush){
        this->flush();
    }
}

void os::Capabilites::clear(bool autoflush)
{
    if( cap_clear(m_caps) == -1){
        throw os::ExcOs(__func__);
    }
    if(autoflush){
        this->flush();
    }
}

/// see also: cap_set_proc
void os::Capabilites::flushToProc()
{
    if (::cap_set_proc(m_caps) == -1){
        throw os::ExcOs(__func__);
    }
}


/// see also cap_get_proc
os::Capabilites::Ptr_t os::Capabilites::fromProc()
{
    cap_t caps = ::cap_get_proc();
    if(caps == nullptr){
        throw os::ExcOs(__func__);
    }
    return Ptr_t(new os::Capabilites(caps));
}
