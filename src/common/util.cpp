

#include <uuid/uuid.h>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <execinfo.h>

#include "util.h"
#include "nullable_value.h"

#include "os.h"

/// stream.readLineInto added in qt 5.5, be backwards compatible...
bool readLineInto(QTextStream& stream, QString *line, qint64 maxlen){
    #if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
    QString str =  stream.readLine(maxlen);
    if(line != nullptr){
        *line = str;
    }
    return ! str.isNull();
#else
    return stream.readLineInto(line, maxlen);
#endif
}



#if QT_VERSION < QT_VERSION_CHECK(5, 6, 0)
QTextStream& operator<<(QTextStream& stream, const QStringRef &string){
    for(int i=0; i < string.size(); i++){
        (stream) << string.at(i);
    }
    return stream;
}
#endif



bool registerQtConversionStuff()
{
    return QMetaType::registerConverter<QString, std::string>( [](const QString& str){
        return str.toStdString();
    }) &&  QMetaType::registerConverter<std::string, QString>( [](const std::string& str){
        return QString::fromStdString(str);
    }) && QMetaType::registerConverter<HashValue, QString>( [](const HashValue& val){
        return ((val.isNull()) ? QString() : QString::number(val.value()));
    }) && QMetaType::registerConverter<QString, HashValue>( [](const QString& val){
        return ((val.isEmpty()) ? HashValue() : HashValue(qVariantTo_throw<HashValue::value_type>(val)));
    }) ;
}


QDebug &operator<<(QDebug &out, const std::string &str)
{
    out << str.c_str();
    return out;
}


void bytesCombine(std::string &){}

/// Find out, if fullstring ends with ending
bool hasEnding(const std::string &fullString, const std::string &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    }
    return false;

}


/// Same as QFileInfo::absoluteFilePath but additionaly
/// strips a trailing slash, if any, except the path is only
/// root /
QString absPath(const QString &path)
{
    if(path == "/"){
        return path;
    }
    QFileInfo inf(path);
    QString abs = inf.absoluteFilePath();
    if(abs.endsWith(QDir::separator())){
        abs = abs.left(abs.length() - 1);
    }
    return abs;
}

/// Equivalent of python's uuid1:  'import uuid; print(uuid.uuid1())'
/// @param madeSafe: pass a bool to know afterwards, whether
/// the uuid was created in a safe way.
QByteArray make_uuid(bool *madeSafe){
    QByteArray uuid;
    uuid.resize(sizeof (uuid_t));

    int ret = uuid_generate_time_safe(
                static_cast<uchar*>(static_cast<void*>(uuid.data()))) ;
    if(madeSafe != nullptr){
        *madeSafe = ret == 0;
    }
    return uuid;
}

char *strDataAccess(std::string &str){
    return &str[0];
}

const char *strDataAccess(const std::string &str){
    return str.c_str();
}


char *strDataAccess(QByteArray &str){
    return str.data();
}
const char* strDataAccess(const QByteArray& str){
    return str.constData();
}




/// Constructs a string containing "null", if cstr is null,
/// else the respective value
std::string strFromCString(const char *cstr)
{
    if(cstr == nullptr) return "null";
    return  cstr;
}

QPair<std::string, std::string> splitAbsPath(const std::string &fullPath)
{
    char sep = '/';
#ifdef _WIN32
    sep = '\\';
#endif

    size_t i = fullPath.rfind(sep, fullPath.length());
    QPair<std::string, std::string> pair;
    if(i == std::string::npos){
        pair.first = fullPath;
        return pair;
    }
    if(i == 0){
        pair.first = "/";
        pair.second = fullPath.substr(1, fullPath.length() - 1);
        return pair;
    }
    pair.first = fullPath.substr(0, i);
    pair.second = fullPath.substr(i+1, fullPath.length() - i);
    return pair;
}


/// Hidden files start with a dot, the the first dot is ignored
/// ( the part before the last dot must not be empty or no file extension
/// returned)
/// @param fname: must not contain any os-separator (e.g. /)
std::string getFileExtension(const std::string &fname)
{
    const auto dotIdx = fname.find_last_of('.');
    if(dotIdx != std::string::npos && dotIdx != 0){
        return fname.substr(dotIdx +1);
    }
    return "";
}

// maybe_todo: add #IF GCC here to allow for other compiler...
/// @param startIdx: generally you would want to choose a value
/// greater than zero, otherwise this function will be added as well.
std::string generate_trace_string(int startIdx)
{
    const int MAX_STACKTRACE_SIZE = 10;

    void *array[MAX_STACKTRACE_SIZE];
    char **strings;

    auto size = backtrace (array, MAX_STACKTRACE_SIZE);
    strings = backtrace_symbols (array, size);

    std::string bt;
    for (int i = startIdx; i < size; i++){
        bt += std::string(" at ") + strings[i] + "\n";
    }
    free (strings);
    return bt;
}

QString argvToQStr(int argc, char * const argv[]){
    QStringList l;
    for(int i=0; i < argc; i++){
        l.push_back(argv[i]);
    }
    return l.join(" ");
}

std::string argvToStr(int argc, char *const argv[])
{
    std::string argStr;
    for(int i=0; i < argc; i++){
        argStr += std::string(argv[i]) + ' ';
    }
    if(! argStr.empty()){
        // strip final whitespace
        argStr.resize(argStr.size() - 1);
    }
    return argStr;
}

/// Argv to space-separated string
/// As usual, argv must be terminated by a final nullptr.
std::string argvToStr(char *const argv[])
{
    std::string argStr;
    while(true){
        if(*argv == nullptr){
            break;
        }
        argStr += std::string(*argv) + ' ';
        ++argv;
    }
    if(! argStr.empty()){
        // strip final whitespace
        argStr.resize(argStr.size() - 1);
    }
    return argStr;
}

/// see also: QChar::isSpace
int indexOfNonWhiteSpace(const QString &str)
{
    for(int i=0; i < str.size(); i++){
        if(! str[i].isSpace()){
            return i;
        }
    }
    return -1;
}



bool qVariantTo(const std::string &str, QString *result) {
    *result = QString::fromStdString(str);
    return true;

}
