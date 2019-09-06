
#include <QDebug>
#include <cassert>
#include <unistd.h>
#include <sys/ioctl.h>


#include "qoptargparse.h"
#include "qoptsqlarg.h"
#include "qoptvarlenarg.h"
#include "excoptargparse.h"
#include "cleanupresource.h"
#include "qoutstream.h"
#include "cpp_exit.h"
#include "qformattedstream.h"

using RawValues_t = QOptArg::RawValues_t;

namespace  {



/// Consume the next arguments according to the found trigger (if any).
/// Store the found values in arg
void consumeOptArgs(int argc, char *argv[], int& i, QOptArg& arg){

    auto & optionalTrigger = arg.optTrigger();
    QString preprocessedTrigger = arg.preprocessTrigger(argv[i]);

    auto foundTrigger = optionalTrigger.trigger().find(preprocessedTrigger);
    if(foundTrigger != optionalTrigger.trigger().end()) {
        arg.setParsedTrigger(preprocessedTrigger);
        // trigger given and consumed -> head to next arg
        ++i;
    } else {
        // The trigger is optional, probably it was leaved out (or mistyped).
        // In that case, the default trigger is used.
        // Note that the default trigger *must* be part of the allowed
        // trigger-set.
        foundTrigger = optionalTrigger.trigger().find(arg.defaultTriggerStr());
        assert(foundTrigger != optionalTrigger.trigger().end());
    }

    // Depending on the trigger a different count of values can be consumed
    RawValues_t v;
    v.argv = &argv[i];

    for(v.len = 0; v.len < foundTrigger.value(); v.len++){
        if(i >= argc){
            // Out of args - delegate it to business logic
            break;
        }
        // This parser does *not* support empty commandline arguments.
        if(argv[i][0] == '\0'){
            throw ExcOptArgParse(qtr("%1 has an empty value").arg(arg.name()));
        }
        ++i;
    }

    arg.setVals(v);
}


/// A var-len argument starts with the count of following arguments, which shall be consumed
void consumeVarLenArg(int argc, char *argv[], int& i, QOptVarLenArg* arg){
    const char* nArgStr = argv[i];
    int nArgs;
    try {
        qVariantTo_throw(nArgStr, &nArgs, false);
    } catch (const ExcQVariantConvert&) {
        throw ExcOptArgParse(qtr("The argument %1 expects an integer as first value "
                                 "but %2 was given").arg(arg->name(), nArgStr));
    }
    // increment for nArgStr and all following values
    RawValues_t v;
    v.argv = &argv[++i];
    v.len = nArgs;

    i += nArgs;
    if(i > argc){
         throw ExcOptArgParse(qtr("%1: too few arguments left (%2 required)")
                              .arg(arg->name()).arg(nArgs));
    }
    arg->setVals(v);
}

} // namespace




QOptArgParse::QOptArgParse() = default;


void QOptArgParse::addArg(QOptArg *arg)
{
    assert(m_args.find(arg->name()) == m_args.end());

    m_args.insert({arg->name(), arg});
    if(! arg->shortName().isEmpty()){
        // short names are optional in which case they are empty
        m_argsShort.insert({arg->shortName(), arg});
    }
}

/// Parse the commandline for all previously added arguments (and for
/// -h, --help, after which the application EXITS).
/// Parsing starts at argv[0], so in case it was received from the parent process,
/// rather increment it first (++argv; argc--;)
/// @throws ExcOptArgParse
void QOptArgParse::parse(int argc, char *argv[])
{
    // to know which args we got, create a copy and delete
    // elements on match
    auto argsCopy = m_args;

    if(argc > 0){
        QByteArray first(argv[0]);
        if(first == "-h" || first == "--help"){
            printHelp();
            cpp_exit(0);
        }
    }
    // generate the vector here and not in addArg to allow
    // for adding requirements *after* an argument was added
    // to the parser
    QVector<const QOptArg*> argsWithRequirements;

    for(int i=0; i < argc; ){
        tsl::ordered_map<QString, QOptArg*>::iterator argIter;
        const QString argStr = argv[i];

        if(argStr.startsWith("--")){
            // search in long names
            argIter = argsCopy.find(argStr);
        } else {
            // search in short names but then try to refind it in argsCopy (long names),
            // to know, which args we got
            argIter = m_argsShort.find(argStr);
            if(argIter == m_argsShort.end()){
                argIter = argsCopy.end();
            } else {
                argIter = argsCopy.find(argIter.value()->name());
            }
        }

        if(argIter == argsCopy.end()){
            if(m_args.find(argStr) != m_args.end()){
                // maybe_todo: add mulit arg, if required and perform dynamic_cast to that
                // subclass
                throw ExcOptArgParse(argStr + qtr(" was passed multiple times"));
            }
            // We are done. Store rest-ptr
            m_rest.argv = &argv[i];
            m_rest.len = argc - i;

            break;
        }
        // remember that we got this arg
        auto deleteArgLater = finally([&argsCopy, &argIter] {
            argsCopy.erase(argIter);
        });

        QOptArg* arg = argIter.value();
        arg->setArgIdx(i);
        if(! arg->requiredArs().isEmpty()){
            argsWithRequirements.push_back(arg);
        }

        if(! arg->hasValue()){
            // a simple flag
            if(arg->isFinalizeFlag()){
                // We are done. Store rest-ptr
                ++i;
                m_rest.argv = &argv[i];
                m_rest.len = argc - i;
                if(m_rest.len == 0){
                    throw ExcOptArgParse(qtr("'%1' passed without further arguments").arg(arg->name()));
                }
                break;
            }
            ++i;
            continue;
        }
        if(++i >= argc){
            throw ExcOptArgParse(qtr("Missing value for %1").arg(arg->name()));
        }
        if(argv[i][0] == '\0'){
            // This parser does *not* support empty commandline arguments.
            throw ExcOptArgParse(qtr("%1 has an empty value").arg(arg->name()));
        }
        auto* varLenArg = dynamic_cast<QOptVarLenArg*>(arg);
        // each of below cases has to point i to the next argument to be parsed!
        if(varLenArg != nullptr){
            consumeVarLenArg(argc, argv, i, varLenArg);
        } else if(! arg->optTrigger().isEmpty()){
            // special "feature" of this parser: consume the next argument(s) according
            // to the given, xor to the default trigger word.
            consumeOptArgs(argc, argv, i, *arg);
        } else {
            RawValues_t v;
            v.argv = &argv[i];
            v.len = 1;
            arg->setVals(v);
            ++i;
        }

    }

    for(const QOptArg* argWithReq : argsWithRequirements){
        for(const QOptArg* requiremnt : argWithReq->requiredArs()){
            if(! requiremnt->wasParsed()){
                throw ExcOptArgParse(qtr("'%1' is required by '%2' but was not parsed.")
                                     .arg(requiremnt->name(), argWithReq->name()));
            }
        }
    }



}

QOptArg::RawValues_t &QOptArgParse::rest()
{
    return m_rest;
}


void QOptArgParse::setHelpIntroduction(const QString &txt)
{
    m_helpIntroduction = txt;
}

void QOptArgParse::printHelp()
{
    QFormattedStream s(stdout);

    struct winsize termWinSize;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &termWinSize);
    if(termWinSize.ws_col > 10 &&  termWinSize.ws_col < 80 ){
        s.setMaxLineWidth(termWinSize.ws_col);
    } else {
        s.setMaxLineWidth(80);
    }

    s << m_helpIntroduction
      << "\n-h, --help :"
      << qtr("Print this help and exit") << "\n";

    const QString indent = "      ";
    for(const auto &nameArgPair : m_args){
        if(nameArgPair.second->internalOnly()){
            continue;
        }
        QString shortNameStr = nameArgPair.second->shortName();
        if(! shortNameStr.isEmpty()){
            shortNameStr += ", ";
        }
        QString value;
        if(nameArgPair.second->hasValue()){
            if(nameArgPair.second->allowedOptions().empty()){
                // first two characters are --
                value =nameArgPair.second->name()[2];
            } else {
                for(const QString& str : nameArgPair.second->allowedOptions()){
                    value += str + nameArgPair.second->allowedOptionsDelimeter();
                }
                value.resize(value.size() - nameArgPair.second->allowedOptionsDelimeter().size());
            }
        }
        s << shortNameStr << nameArgPair.second->name();
        s.setLineStart(indent);
        s << value << ": " << nameArgPair.second->description() << "\n";
        s.setLineStart("");

    }
}
