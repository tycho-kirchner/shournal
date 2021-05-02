
#include <cstdlib>

#include <QString>
#include <QCoreApplication>
#include <QStandardPaths>

#include "qoutstream.h"
#include "app.h"
#include "util.h"
#include "osutil.h"

// may be supplied at buildtime, else should be defined in cmake file
#ifndef SHOURNAL_MSENTERGROUP
static_assert (false, "SHOURNAL_MSENTERGROUP not defined");
#endif


const char* app::CURRENT_NAME = "UNDEFINED";
const char* app::SHOURNAL = "shournal";
const char* app::SHOURNAL_RUN = "shournal-run";
const char* app::SHOURNALK_DKMS = "shournalk-dkms";
const char* app::SHOURNAL_RUN_FANOTIFY = "shournal-run-fanotify";
// groupnames should be smaller than 16 characters (portability).
const char* app::MSENTER_ONLY_GROUP = SHOURNAL_MSENTERGROUP; // defined in cmake
const char* app::ENV_VAR_SOCKET_NB = "_SHOURNAL_SOCKET_NB";


static bool g_inIntegrationTestMode=false;


void app::setupNameAndVersion(const char* currentName)
{
    app::CURRENT_NAME = currentName;
    QIErr::setPreambleCallback([]() { return QString(app::CURRENT_NAME) + ": "; });

    g_inIntegrationTestMode = getenv("_SHOURNAL_IN_INTEGRATION_TEST_MODE") != nullptr;
    QString integrationSuffix;

    if(g_inIntegrationTestMode){
        integrationSuffix = "-integration-test";
        QStandardPaths::setTestModeEnabled(true);
        // QIErr() << "running in integration test mode";
    }
    QCoreApplication::setApplicationName(app::SHOURNAL + integrationSuffix);
    QCoreApplication::setApplicationVersion(app::version().toString());

}

bool app::inIntegrationTestMode()
{
    return g_inIntegrationTestMode;
}

int app::findIntegrationTestFd(){
    if(! app::inIntegrationTestMode()){
        return -1;
    }
    QByteArray fdStr = getenv("_SHOURNAL_INTEGRATION_TEST_PIPE_FD");
    if(fdStr.isNull()){
        QIErr() << "Although in integration-test, cannot"
                      "find pipe fd in env!";
        return -1;
    }
    int fd = qVariantTo_throw<int>(fdStr);
    if(! osutil::fdIsOpen(fd)){
        QIErr() << "_SHOURNAL_INTEGRATION_TEST_PIPE_FD is not open - number:"
                << fd;
        return -1;
    }
    return fd;
}

const QVersionNumber &app::version()
{
    // defined in cmake
    static const QVersionNumber v = QVersionNumber::fromString(SHOURNAL_VERSION);
    return v;
}

const QVersionNumber &app::initialVersion()
{
    static const QVersionNumber v = QVersionNumber{0, 1}; // first version ever;
    return v;
}


