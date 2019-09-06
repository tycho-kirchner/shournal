#pragma once

#include <iostream>

#include "util.h"

namespace private_namesapce {


template <typename F>
struct CleanupResource
{
    CleanupResource(F f, bool enable) :
        m_cleanF{f},
        m_enabled(enable)
    {}

    ~CleanupResource() {
        // Do not throw from destructor
        try {
            if(m_enabled){
                m_cleanF();
            }
        } catch (const std::exception& ex ) {
            std::cerr << ex.what() << "\n";
        }
    }

    void setEnabled(bool val){
        m_enabled = val;
    }

public:
    Q_DISABLE_COPY(CleanupResource)
    DEFAULT_MOVE(CleanupResource)

private:
    F m_cleanF;
    bool m_enabled;
};


} // namespace private_namesapce


template <typename F>
private_namesapce::CleanupResource<F> finally(F f, bool enable=true) __attribute__ ((warn_unused_result));


/// Perform a final action before leaving the block:
/// Usage:
/// char* buf = new char;
/// auto deleter = finally([buf] {delete buf; });
template <typename F>
private_namesapce::CleanupResource<F> finally(F f, bool enable){
    return private_namesapce::CleanupResource<F>(f, enable);
}
