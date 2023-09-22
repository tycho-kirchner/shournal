#pragma once

#include <string>
#include <vector>

#include "fileeventhandler.h"
#include "util.h"

struct fanotify_event_metadata;

class FanotifyController
{
public:
    FanotifyController();
    ~FanotifyController();
    void setFileEventHandler(std::shared_ptr<FileEventHandler>&);
    void setupPaths();

    bool handleEvents();

    int fanFd() const;

    int getFanotifyMaxEventCount() const;
    uint getOverflowCount() const;

public:
    Q_DISABLE_COPY(FanotifyController)
    DISABLE_MOVE(FanotifyController)


private:

    void handleSingleEvent(const fanotify_event_metadata &metadata);
    void handleCloseRead_safe(const fanotify_event_metadata &metadata);
    void handleModCloseWrite_safe(const fanotify_event_metadata &metadata);
    void unregisterAllReadPaths();
    void ignoreOwnPath(const QByteArray& p);

    std::shared_ptr<FileEventHandler> m_feventHandler;

    uint m_overflowCount{0};
    int m_fanFd;
    bool m_markLimitReached;
    bool m_ReadEventsUnregistered;
    std::vector<std::string> m_readMountPaths; // all mount paths initially marked for read-events
    const Settings::WriteFileSettings& r_wCfg;
    const Settings::ReadFileSettings& r_rCfg;
    const Settings::ScriptFileSettings& r_scriptCfg;

};

