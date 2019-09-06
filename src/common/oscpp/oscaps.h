#pragma once

#include <sys/capability.h>
#include <memory>
#include <vector>

#include "util.h"

namespace os {


/// Simple wrapper around libcap
/// When leaving autoflush to the default value of true, the set flags are applied
/// immediately to the process (throws ExsOs on error).
class Capabilites{
public:
    typedef std::shared_ptr<Capabilites> Ptr_t;
    typedef std::vector<cap_value_t> CapFlags;

    ~Capabilites();

    void setFlags(cap_flag_t typeOfFlag ,const CapFlags& flags, bool autoflush=true);
    void clearFlags(cap_flag_t typeOfFlag ,const CapFlags& flags, bool autoflush=true);

    void clear(bool autoflush=true);

    void flushToProc();

    static Ptr_t fromProc();

public:
    Q_DISABLE_COPY(Capabilites)
    DEFAULT_MOVE(Capabilites)

private:
    Capabilites(cap_t caps);
    void flush();

    cap_t m_caps;
};

}
