#pragma once

#include <QVersionNumber>
#include <unordered_set>

namespace app {

const extern char* CURRENT_NAME;
const extern char* SHOURNAL;
const extern char* SHOURNAL_RUN;
const extern char* SHOURNAL_RUN_FANOTIFY;
const extern char* MSENTER_ONLY_GROUP;
const extern char* ENV_VAR_SOCKET_NB;

const extern std::unordered_set<QString> &VERBOSITIES;

void setupNameAndVersion(const char *currentName);
bool inIntegrationTestMode();
int findIntegrationTestFd();

const QVersionNumber& version();
const QVersionNumber& initialVersion();


}



