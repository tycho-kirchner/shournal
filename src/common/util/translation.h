#pragma once

#include <QString>
#include <cerrno>

#include "util.h"

namespace translation {
    bool init();

    char* strerror_l(int errorNumber=errno);


    class TrSnippets {
    public:


        const QString enable {qtr("enable")};
        const QString shournalShellIntegration {qtr("shournal shell-integration")};
        const QString shournalRestore {qtr("shournal-restore")};

        static TrSnippets &instance();

    public:
        Q_DISABLE_COPY(TrSnippets)
        ~TrSnippets() = default;
    private:
        TrSnippets() = default;
    };

} // namespace translation


