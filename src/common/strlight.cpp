
#include <cassert>

#include "strlight.h"


/// Set interal buffer and size. Only allowed
/// if *this was constructed via the default constructor.
void StrLight::setRawData(const char *buf, StrLight::size_type n){
    assert( ! m_weOwnBuf || m_buf == nullptr );
    // we won't change buf, promised (;
    char* b = const_cast<char*>(buf);
    m_buf = b;
    m_size = n;
}

void StrLight::setRawSize(StrLight::size_type n){
    assert( ! m_weOwnBuf || m_buf == nullptr );
    m_size = n;
}

/// Allocate count chars and initialize with ch
StrLight::StrLight(StrLight::size_type count, char ch)
{
    allocatePlusX(count);
    memset(m_buf, ch, count);
    setSizeInternal(count);
}

/// Allocates memory, creates copy of nullterminated cstring.
/// @param cstring *must not be null*.
StrLight::StrLight(const char *cstring) :
    StrLight(cstring, (cstring == nullptr) ? 0 : strlen(cstring))
{}

/// Allocates memory, creates copy of cstring
StrLight::StrLight(const char *cstring, StrLight::size_type size)
{
    if(cstring == nullptr){
        throw QExcProgramming("cstring == nullptr");
    }
    allocatePlusX(size);
    memcpy(m_buf, cstring, size);
    setSizeInternal(size);
}

StrLight::~StrLight(){
    if(m_weOwnBuf && m_buf != nullptr){
        delete[] m_buf;
    }
}

/// Warning: if buffer is non-null, copying is only allowed if
/// buf is managed internally (owned by us). The rationale
/// is e.g. to no accidantially store a StrLight with
/// external buffer in a container. On the other hand
/// it could be supported to copy the external buffer instead,
/// however, a raw buffer is probably set for performance reasons
/// so avoid implicit copying (see also deepCopy)
StrLight::StrLight(const StrLight &other)
{
    if(other.m_buf == nullptr){
        // Nothing to copy
        return;
    }
    if(! other.m_weOwnBuf){
        throw QExcProgramming("Copy constructor called for "
                              "externally managed buffer");
    }
    // copy
    allocatePlusX(other.m_size);
    memcpy(m_buf, other.constData(), other.m_size);
    setSizeInternal(other.m_size);
}

/// Explicit function to also allow for
/// easy copying of StrLight's with externally
/// managed buffer.
StrLight StrLight::deepCopy() const {
    if(m_buf == nullptr){
        return StrLight();
    }
    // copy the data: works for internal
    // and external managed buffer.
    // Do not call copy-constructor here!
    StrLight str(m_buf, m_size);
    return str;
}

StrLight &StrLight::operator=(StrLight other)
{
    swap(*this, other);
    return *this;
}

StrLight &StrLight::operator=(char c)
{
    this->resize(1);
    *m_buf = c;
    return *this;
}

StrLight &StrLight::operator+=(const StrLight &rhs){
    append(rhs.constData(), rhs.size());
    return *this;
}

StrLight &StrLight::operator+=(const char rhs){
    char buf[1];
    buf[0] = rhs;
    append(buf, 1);
    return *this;
}

void StrLight::append(const char *str, StrLight::size_type n){
    auto oldsize = m_size;
    this->resize( m_size + n);
    memcpy(&m_buf[oldsize], str, n);
    assert(m_buf[m_size] == '\0'); // should be done during resize
}

const char& StrLight::operator[](StrLight::size_type idx) const{
    assert(idx < m_size);
    return m_buf[idx];
}



/// move constructor
StrLight::StrLight(StrLight &&other)
    : StrLight()
{
    swap(*this, other);
}

char StrLight::back() const {
    assert(m_size > 0);
    return m_buf[m_size - 1];
}

/// @return true, if *this object owns
/// the buffer. Else, the buffer was passed
/// from outside.
bool StrLight::bufIsManagedByThis() const {
    return m_weOwnBuf;
}

StrLight::size_type StrLight::find(const char *s, StrLight::size_type pos) const
{
    assert(m_buf != nullptr);
    assert(pos < m_size);
    const char* haystackStart = &m_buf[pos];
    const char* match = strstr(haystackStart, s);
    if(match == nullptr) {
        return StrLight::npos;
    }
    return match - m_buf;
}

StrLight::size_type StrLight::find(const StrLight &s, StrLight::size_type pos) const
{
    return StrLight::find(s.constData(), pos);
}

/// Find the last occurence of ch in str.
/// @return the found index
int StrLight::lastIndexOf(char ch) const {
    for(ssize_type i=m_size - 1; i >= 0; i--){
        assert(m_buf != nullptr);
        if(m_buf[i] == ch){
            return int(i);
        }
    }
    return -1;
}

/// See QbyteArray::left
StrLight StrLight::left(int len) const {
    StrLight s(m_buf, std::min<size_type>(len, m_size));
    return s;
}

/// See QbyteArray::mid
StrLight StrLight::mid(int pos) const {
    StrLight s(&m_buf[pos], m_size - pos);
    return s;
}

bool StrLight::empty() const {
    return m_size == 0;
}

StrLight::size_type StrLight::capacity() const {
    return m_capacity;
}

/// Warning: only allowed, if we manage the non-null buffer ourselves.
/// If it is null, create a new buffer.
void StrLight::resize(StrLight::size_type n){
    reserve(n);
    setSizeInternal(n);
}

void StrLight::reserve(StrLight::size_type n)
{
    if(n < m_capacity){
        // capacity nonzero -> buf cannot be null
        assert(m_buf != nullptr);
        assert(m_weOwnBuf);
        return;
    }
    if(m_buf == nullptr){
        // no external buffer is set: allocate our own
        allocatePlusX(n);
    } else {
        assert(m_weOwnBuf);
        realloc(n + 64);
    }
}

const char *StrLight::constData() const {
    return m_buf;
}

char *StrLight::data() {
    return m_buf;
}

const char *StrLight::c_str() const {
    return constData();
}

/// Warning: only allowed, if not null and not empty.
/// Returns pointer to the final char
const char *StrLight::constDataEnd() const
{
    assert(m_buf != nullptr);
    assert(! this->empty());
    return m_buf + m_size - 1;
}

StrLight::size_type StrLight::size() const {
    return m_size;
}

void StrLight::pop_back(){
    assert(m_size > 0);
    this->resize(m_size - 1);
}



/////////// Private ///////////


void StrLight::realloc(StrLight::size_type newCapacity)
{
    assert(m_buf != nullptr);
    assert(m_weOwnBuf);
    char* newArr = new char[newCapacity];
    memcpy(newArr, m_buf, m_size);
    delete[] m_buf;
    m_buf = newArr;
    m_capacity = newCapacity;
    m_weOwnBuf = true;
}

/// allocate a little more than needed
void StrLight::allocatePlusX(StrLight::size_type approxNewCapacity)
{
    assert(m_buf == nullptr);
    // Do not change - other functions rely on this
    approxNewCapacity += 256;
    m_buf = new char[approxNewCapacity];
    m_capacity = approxNewCapacity;
    m_weOwnBuf = true;
}

/// setting buffersize only allowed if we manage it
void StrLight::setSizeInternal(StrLight::size_type n)
{
    assert(m_buf != nullptr);
    assert(m_weOwnBuf);
    m_size = n;
    // otherwise the following would be illegal:
    assert(m_capacity > m_size);
    m_buf[n] = '\0';
}

void swap(StrLight &first, StrLight &second)
{
    using std::swap;

    swap(first.m_weOwnBuf, second.m_weOwnBuf);
    swap(first.m_buf, second.m_buf);
    swap(first.m_size, second.m_size);
    swap(first.m_capacity, second.m_capacity);
}


/////////// General ///////////

uint qHash(const StrLight &key, uint seed)
{
    if (key.size() == 0)
        return seed;
    else
        return qHashBits(key.constData(), key.size(), seed);
}

bool operator==(const StrLight &lhs, const StrLight &rhs) {
    if(lhs.size() != rhs.size()){
        return false;
    }
    if(lhs.size() == 0){
        // both empty
        return true;
    }
    return memcmp(lhs.constData(), rhs.constData(), lhs.size()) == 0;
}

bool operator==(const StrLight &lhs, const char &c)
{
    return lhs.size() == 1 && *lhs.constData() == c;
}

bool operator!=(const StrLight &lhs, const char &c)
{
    return !(lhs == c);
}


QDebug operator<<(QDebug debug, const StrLight &c)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << QByteArray::fromRawData(c.constData(), int(c.size()));
    return debug;
}



const StrLight operator+(const StrLight &s1, const StrLight &s2)
{
    StrLight res;
    res.reserve(s1.size() + s2.size());
    res.append(s1.constData(), s1.size());
    res.append(s2.constData(), s2.size());
    return res;
}

const StrLight operator+(const StrLight &s1, const char &c)
{
    StrLight res;
    res.reserve(s1.size() + 1);
    res.append(s1.constData(), s1.size());
    res += c;
    return res;
}
