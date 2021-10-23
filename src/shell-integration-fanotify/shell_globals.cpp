
#include "app.h"
#include "qoutstream.h"
#include "shell_globals.h"
#include "shell_logger.h"
#include "staticinitializer.h"
#include "translation.h"

const char* ENV_VARNAME_SHELL_VERBOSITY = "_SHOURNAL_LIB_SHELL_VERBOSITY";

static bool updateShouranlRunVerbosityFromEnv(bool verboseIfUnset){
    const char* VERB_VARNAME = "_SHOURNAL_VERBOSITY";
    const char* verbosityValue = getenv(VERB_VARNAME);
    if(verbosityValue == nullptr){
        if(verboseIfUnset){
            logWarning << qtr("Required verbosity environment variable '%1' "
                              "is unset.").arg(VERB_VARNAME);
            return false;
        }
        return true;
    }
    if(app::VERBOSITIES.find(verbosityValue) == app::VERBOSITIES.end()){
        logWarning << qtr("Verbosity environment variable '%1' "
                          "is invalid ('%2')").arg(VERB_VARNAME, verbosityValue);
        return false;
    }
    auto& g_shell = ShellGlobals::instance();
    g_shell.shournalRunVerbosity = verbosityValue;
    return true;
}


ShellGlobals &ShellGlobals::instance()
{
    static ShellGlobals s;
    return s;
}

ShellGlobals::ShellGlobals()
{
     ignoreEvents.clear();
     ignoreSigation.clear();
}

bool ShellGlobals::performBasicInitIfNeeded(){
    bool success = true;
    static StaticInitializer initOnFirstCall( [&success](){
        app::setupNameAndVersion("shournal shell-integration");
        try {
            if(! shournal_common_init()){
                QIErr()  << qtr("Fatal error: failed to initialize custom Qt conversion functions");
            }
            shell_logger::setup();
            translation::init();
            updateVerbosityFromEnv(false);
        } catch (const std::exception& ex) {
            success = false;
            logCritical << ex.what();
        }

     });
    return success;
}


/// @param verboseIfUnset If true print a warning if the environment variables
/// are unset.
/// @return if verboseIfUnset: return false if unset or invalid
///         else             : return false if invalid
bool ShellGlobals::updateVerbosityFromEnv(bool verboseIfUnset){
    bool shournalRunSuccess = updateShouranlRunVerbosityFromEnv(verboseIfUnset);

    const char* VERB_VARNAME = ENV_VARNAME_SHELL_VERBOSITY;
    const char* verbosityValue = getenv(VERB_VARNAME);
    if(verbosityValue == nullptr){
        if(verboseIfUnset){
            logWarning << qtr("Required verbosity environment variable '%1' "
                              "is unset.").arg(VERB_VARNAME);
            return false;
        }
        return shournalRunSuccess;
    }
    if(app::VERBOSITIES.find(verbosityValue) == app::VERBOSITIES.end()){
        logWarning << qtr("Verbosity environment variable '%1' "
                          "is invalid ('%2')").arg(VERB_VARNAME, verbosityValue);
        return false;
    }
    auto& g_shell = ShellGlobals::instance();
    g_shell.verbosityLevel = logger::strToMsgType(verbosityValue);

    return shournalRunSuccess;
}

