
#include <cstdlib>

#include <QString>
#include <QCoreApplication>
#include <QStandardPaths>

#include "qoutstream.h"
#include "app.h"
#include "util.h"

// may be supplied at buildtime, else should be defined in cmake file
#ifndef SHOURNAL_MSENTERGROUP
static_assert (false, "SHOURNAL_MSENTERGROUP not defined");
#endif


const char* app::SHOURNAL = "shournal";
const char* app::SHOURNAL_RUN = "shournal-run";
// groupnames should be smaller than 16 characters (portability).
const char* app::MSENTER_ONLY_GROUP = SHOURNAL_MSENTERGROUP; // defined in cmake
const char* app::ENV_VAR_SOCKET_NB = "_SHOURNAL_SOCKET_NB";

static bool g_inIntegrationTestMode=false;


void app::setupNameAndVersion()
{
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


const QVersionNumber &app::version()
{
    // defined in cmake
    static const QVersionNumber v = QVersionNumber::fromString(SHOURNAL_VERSION);
    return v;
}
