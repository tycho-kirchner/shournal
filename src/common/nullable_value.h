#pragma once


#include "exccommon.h"


class QExcNullDeref : public QExcCommon
{
public:
    using QExcCommon::QExcCommon;
};


template <class T>
class NullableValue {
public:

    typedef T value_type;

    NullableValue() : m_isNull(true){}
    NullableValue(const T& t) : m_isNull(false), m_value(t){}

    const T& value() const {
        if(m_isNull){
            throw QExcNullDeref("Tried to obtain value while it is set to null");
        }
        return m_value;
    }

    void setValue(const T& val){
        m_value = val;
        m_isNull = false;
    }

    bool isNull() const {
        return m_isNull;
    }

    void setNull(){
        m_isNull = true;
    }

    bool operator==(const NullableValue& rhs) const
    {
        if(isNull()){
            return rhs.isNull();
        }
        // we are not null
        if( rhs.isNull()){
            return false;
        }
        // other is also not null.
        // compare vals
        return value() == rhs.value();
    }

    NullableValue& operator=(const T& val)
    {
        setValue(val);
        return *this;
    }

protected:
    bool m_isNull;
    T m_value;
};


template <class T>
bool operator==(const T& lhs, const NullableValue<T>& rhs) {
    // comparing to value, so it cannot be null
    if(rhs.isNull()){
        return false;
    }
    return lhs == rhs.value();
}


template <class T>
bool operator==(const NullableValue<T>& lhs, const T& rhs) {
    return rhs == lhs.value();
}

typedef NullableValue<uint64_t> HashValue;


