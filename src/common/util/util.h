
#pragma once

#ifndef qtr
#define qtr QObject::tr
#endif

#define GET_VARIABLE_NAME(Variable) (#Variable)

#include <assert.h>

#include <QDateTime>
#include <QtGlobal>
#include <QObject>
#include <QVariant>
#include <QString>
#include <QDir>
#include <QTextStream>

#include <functional>
#include <memory>

#include "exccommon.h"
#include "UninitializedMemoryHacks.h"
#include "strlight.h"


FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(signed char)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(unsigned char)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(char16_t)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(char32_t)
FOLLY_DECLARE_STRING_RESIZE_WITHOUT_INIT(unsigned short)


#define DISABLE_MOVE(Class) \
    Class(const Class &&) Q_DECL_EQ_DELETE;\
    Class &operator=(Class &&) Q_DECL_EQ_DELETE;

#define DEFAULT_MOVE(Class) \
    Class(Class &&) noexcept Q_DECL_EQ_DEFAULT;\
    Class &operator=(Class &&) noexcept Q_DECL_EQ_DEFAULT ;

bool readLineInto(QTextStream& stream, QString *line, qint64 maxlen = 0);


#if QT_VERSION < QT_VERSION_CHECK(5, 6, 0)
#include <QTextStream>
QTextStream& operator<<(QTextStream& stream, const QStringRef &string);
#endif


/// @return true, if the given weak pointer is default constructed
template <typename T>
bool is_uninitialized(std::weak_ptr<T> const& weak) {
    using wt = std::weak_ptr<T>;
    return !weak.owner_before(wt{}) && !wt{}.owner_before(weak);
}


Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(StrLight)

bool shournal_common_init();

#if QT_VERSION < QT_VERSION_CHECK (5, 14, 0)
namespace std {

/// Make QString hashable in stl-containers
template<> struct hash<QString> {
    std::size_t operator()(const QString& s) const {
        return qHash(s);
    }
};
}
#endif

StrLight toStrLight(const QString& str);

/// allow std::string to be printed via qDebug()
QDebug& operator<<(QDebug& out, const std::string& str);



/// Common functions to get raw data access for std::string and QByteArray
const char* strDataAccess(const char* str);
char* strDataAccess(char* str);
char* strDataAccess(std::string& str);
const char* strDataAccess(const std::string& str);
char* strDataAccess(QByteArray& str);
const char* strDataAccess(const QByteArray& str);
char* strDataAccess(StrLight& str);
const char* strDataAccess(const StrLight& str);

int indexOfNonWhiteSpace(const QString& str);

/// return argv as space-separated string
QString argvToQStr(int argc, char *const argv[]);

std::string argvToStr(int argc, char * const argv[]);
std::string argvToStr(char * const argv[]);

template<class T>
std::string bytesFromVar(const T& t)
{
    static_assert (std::is_pod<T>(), "");
    const char* raw_ = static_cast<const char*>(static_cast<const void*>(&t));
    std::string str(raw_, sizeof (T));
    return str;
}

/// @return defaultVal if size does not match
template<class T>
T varFromBytes(const std::string &str, const T& defaultVal)
{
    static_assert (std::is_pod<T>(), "");
    if(str.size() != sizeof (T)){
        return defaultVal;
    }
    const T* pT = static_cast<const T*>(
                static_cast<const void*>(&str[0])
            );
    T copy_ = *pT;
    return copy_;
}


template<class T>
QByteArray qBytesFromVar(const T& t)
{
    static_assert (std::is_pod<T>(), "");
    const char* raw_ = static_cast<const char*>(static_cast<const void*>(&t));
    QByteArray str(raw_, sizeof (T));
    return str;
}

/// @return defaultVal if size does not match
template<class T>
T varFromQBytes(const QByteArray &str, const T& defaultVal)
{
    static_assert (std::is_pod<T>(), "");
    if(str.size() != sizeof (T)){
        return defaultVal;
    }
    const T* pT = static_cast<const T*>(
                static_cast<const void*>(str.constData())
            );
    T copy_ = *pT;
    return copy_;
}

template<class T>
T varFromQBytes(const QByteArray &str)
{
    static_assert (std::is_pod<T>(), "");
    if(str.size() != sizeof (T)){
        return T();
    }
    const T* pT = static_cast<const T*>(
                static_cast<const void*>(str.constData())
            );
    T copy_ = *pT;
    return copy_;
}


template<class T> T
BIT(const T & x)
{ return T(1) << x; }

template<class T>
bool IsBitSet(const T & x, const T & y)
{ return (x & y) != 0; }

/// unset mask in flags;
template<class T>
void clearBitIn(T& flags, const T& mask){
    flags &= ~mask;
}

/// set mask in flags;
template<class T>
void setBitIn(T& flags, const T& mask){
    flags |= mask;
}


bool hasEnding(const std::string &fullString, const std::string &ending);

/// recursion end of bytesCombine ...
void bytesCombine(std::string &);

/// Obtain all bytes of all arguments and append them to str
template<typename First, typename ... Values>
void bytesCombine(std::string & result, First arg, const Values&... rest ){
    for (size_t idx = 0; idx < sizeof(First); idx++){
        char byte = *((char *)&arg + idx);
        result.push_back(byte);
    }
    bytesCombine(result, rest...);
}


template <class Container>
bool contains(const Container& container, const typename Container::value_type& element)
{
    return std::find(container.begin(), container.end(), element) != container.end();
}

/// Drop certain fields of the time, e.g. milliseconds. Also
/// sets lower fields, e.g. dropping minutes sets seconds
/// and ms to 0 as well
/// @param c one of M(inutes), s(econds), m(illiseconds)
static inline void dropFromTime(QTime& t, char c){
    int m=t.minute(), s=t.second(), ms=t.msec();
    switch (c) {
    case 'M': m = 0; break;
    case 's': s = 0; break;
    case 'm': ms = 0; break;
    default: throw QExcIllegalArgument(QString("Bad format c %1").arg(c));
    }
    t.setHMS(t.hour(), m, s, ms);
}

/// @overload
static inline void dropFromTime(QDateTime& d, char c){
    auto t = d.time();
    dropFromTime(t, c);
    d.setTime(t);
}

QString absPath(const QString& path);

/// Convert the passed value to QVariant, convert
/// to target-type and return true
/// on success
template<typename T>
bool qVariantTo(QVariant var, T* result) {
    if(! var.convert(qMetaTypeId<T>())){
        return false;
    }
    *result = var.value<T>();
    return true;
}

// Don't forget to register converter functions when adding more types...
bool qVariantTo(const std::string& str, QString* result);
bool qVariantTo(const StrLight& str, QString* result);

class ExcQVariantConvert : public QExcCommon
{
public:
     using QExcCommon::QExcCommon;
};

/// Convert to target-type, throw on error
/// @throws ExcQVariantConvert
template<typename T>
void qVariantTo_throw(const QVariant& src, T* dst, bool collectStacktrace=true)
{
    if(! qVariantTo<T>(src, dst)){
        const char* targetTypeName = QVariant::fromValue(*dst).typeName();
        QString actualTypeName;
        if(targetTypeName == nullptr){
            actualTypeName = "invalid/unknown";
        } else {
            actualTypeName = targetTypeName;
        }
        QString mesg = qtr(
                    "Failed to convert '%1' to type '%2'"
                    ).arg(src.toString()).arg(actualTypeName);
        throw ExcQVariantConvert(mesg, collectStacktrace);
    }
}

/// @overload
template<typename T>
T qVariantTo_throw(const QVariant& src, bool collectStacktrace=true){
    T dst;
    qVariantTo_throw(src, &dst, collectStacktrace);
    return dst;
}

/// @overload
template<typename T>
T qVariantTo_throw(const std::string& src, bool collectStacktrace=true ){
    T dst;
    if(! qVariantTo(src, &dst)){
        // this should never happen, because of having
        // specialized for std::string...
        QString mesg = qtr(
                    "Failed to convert std::string to target type.");
        throw ExcQVariantConvert(mesg, collectStacktrace);
    }
    return dst;
}


QByteArray make_uuid(bool *madeSafe=nullptr);


/// Split an absolute path into directory-path and filename:
/// /home/user/foo -> "/home/user", "foo"
/// If no separator is contained, the full path is returned
/// Works with QByteArray, QString and std::string (see overload)
template<typename T>
QPair<T, T> splitAbsPath(const T& path){
    QPair<T, T> pair;
    const int lastSlash = path.lastIndexOf('/');
    if(lastSlash == -1 || lastSlash == int(path.size()) - 1){
        pair.first = path;
        return pair;
    }
    if(lastSlash == 0){
        pair.first = "/";
        pair.second = (path.mid(1));
        return pair;
    }
    pair.first = path.left(lastSlash);
    pair.second = path.mid(lastSlash + 1);
    return pair;
}


template<typename T>
T pathJoinFilename(const T& path, const T& filename){
    assert(path.size() != 0);
    assert(filename.size() != 0);

    // special case root
    if(path == "/"){
        return path + filename;
    }
    return path + "/" + filename;
}

/// @overload
QPair<std::string, std::string> splitAbsPath(const std::string& fullPath);


std::string getFileExtension(const std::string& fname);

std::string strFromCString(const char* cstr);


std::string generate_trace_string(int startIdx=2);

