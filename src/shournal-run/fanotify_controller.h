#pragma once

#include <string>
#include <vector>

#include "os.h"
#include "fileeventhandler.h"

struct fanotify_event_metadata;

class FanotifyController
{
public:
    FanotifyController(FileEventHandler& feventHandler);
    ~FanotifyController();
    void setupPaths();

    bool handleEvents();

    bool overflowOccurred() const;

    int fanFd() const;

public:

    FanotifyController(const FanotifyController&) = delete;
    void operator=(const FanotifyController&) = delete;

private:

    void handleSingleEvent(const fanotify_event_metadata &metadata);
    void handleCloseNoWrite_safe(const fanotify_event_metadata &metadata);
    void handleModCloseWrite_safe(const fanotify_event_metadata &metadata);
    void unregisterAllReadPaths();
    void ignoreOwnPath(const QByteArray& p);

    FileEventHandler& m_feventHandler;

    bool m_overflowOccurred;
    int m_fanFd;
    bool m_markLimitReached;
    bool m_ReadEventsUnregistered;
    std::vector<std::string> m_readMountPaths; // all mount paths initially marked for read-events

};

