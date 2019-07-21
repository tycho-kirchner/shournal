#pragma once

/// Call an arbitrary function in the constructor, which can
/// be used for static initialization
class StaticInitializer
{
public:
    template<typename Lambda>
    StaticInitializer(Lambda f){
        f();
    }

    ~StaticInitializer(){}
};

