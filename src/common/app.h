#pragma once

#include <QVersionNumber>

namespace app {

const extern char* SHOURNAL;
const extern char* SHOURNAL_RUN;
const extern char* MSENTER_ONLY_GROUP;
const extern char* ENV_VAR_SOCKET_NB;

void setupNameAndVersion();
bool inIntegrationTestMode();

const QVersionNumber& version();
const QVersionNumber& initialVersion();


}



