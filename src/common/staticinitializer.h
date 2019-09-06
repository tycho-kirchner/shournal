#pragma once

#include "util.h"

/// Call an arbitrary function in the constructor, which can
/// be used for static initialization
class StaticInitializer
{
public:
    template<typename Lambda>
    StaticInitializer(Lambda f){
        f();
    }

public:
    ~StaticInitializer() = default;

public:
    Q_DISABLE_COPY(StaticInitializer)
    DEFAULT_MOVE(StaticInitializer)
};

