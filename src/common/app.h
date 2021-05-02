#pragma once

#include <QVersionNumber>

namespace app {

const extern char* CURRENT_NAME;
const extern char* SHOURNAL;
const extern char* SHOURNAL_RUN;
const extern char* SHOURNALK_DKMS;
const extern char* SHOURNAL_RUN_FANOTIFY;
const extern char* MSENTER_ONLY_GROUP;
const extern char* ENV_VAR_SOCKET_NB;

void setupNameAndVersion(const char *currentName);
bool inIntegrationTestMode();
int findIntegrationTestFd();

const QVersionNumber& version();
const QVersionNumber& initialVersion();


}



