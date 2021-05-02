#pragma once

#include <cstring>
#include <qhash.h>
#include <QDebug>

#include "exccommon.h"

/// Yet another String class which aims to perform very fast
/// in critical sections. For example resizing does not initialize memory
/// (other than std::string). Further a "raw"-mode is supported
/// (setRawData, setRawSize), where the caller is responsible
/// for the buffer. Note that the raw mode is only allowed,
/// if StrLight was default-constructed. If Strlight manages
/// the memory (bufIsManagedByThis), setRaw* is prohibited.
/// By using a raw buffer the user clearly shows the intention
/// to care for performance, so in this mode the copy-constructor
/// throws! Use the explicit deepCopy in those cases.
class StrLight {
public:
    typedef size_t size_type;
    typedef ssize_t ssize_type;
    static const size_type	npos = static_cast<size_type>(-1);

    StrLight() = default;
    void setRawData(const char* buf, size_type n);
    void setRawSize(size_type n);

    StrLight(size_type count, char ch);
    StrLight(const char* cstring);
    StrLight(const char* cstring, size_type size);

    ~StrLight();

    StrLight(const StrLight& other);
    StrLight deepCopy() const;
    StrLight(StrLight&& other);

    StrLight& operator=(StrLight other);
    StrLight& operator=(char c);

    StrLight& operator+=(const StrLight& rhs);
    StrLight& operator+=(const char rhs);

    void append(const char* str, size_type n);

    const char& operator[](size_type idx) const;
    char back() const;
    bool bufIsManagedByThis() const;

    size_type find(const char* s, size_type pos = 0) const;
    size_type find(const StrLight& s, size_type pos = 0) const;

    int lastIndexOf(char ch) const;

    StrLight left(int len) const;
    StrLight mid(int pos) const;
    bool empty() const;

    size_type capacity() const;
    size_type size() const;

    void pop_back();
    void resize(size_type n);
    void reserve(size_type n);

    const char *constData() const;
    char *data();
    const char *c_str() const;
    const char* constDataEnd() const;

private:
    void realloc(size_type newCapacity);
    void allocatePlusX(size_type approxNewCapacity);
    void setSizeInternal(size_type n);

    bool m_weOwnBuf {false};
    char* m_buf {nullptr};
    size_type m_size {0};
    size_type m_capacity {0};

public:
    friend void swap(StrLight& first, StrLight& second);
};


uint qHash(const StrLight &key, uint seed = 0);

namespace std {
template<> struct hash<StrLight> {
    std::size_t operator()(const StrLight& s) const {
        return qHash(s);
    }
};
}

const StrLight operator+(const StrLight &s1, const StrLight &s2);
const StrLight operator+(const StrLight &s1, const char &c);

bool operator==(const StrLight &lhs, const StrLight &rhs);
bool operator==(const StrLight &lhs, const char &c);
bool operator!=(const StrLight &lhs, const char &c);

QDebug operator<<(QDebug debug, const StrLight &c);


