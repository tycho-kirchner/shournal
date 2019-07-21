#pragma once

#include <QString>
#include <errno.h>

namespace translation {
    bool init();

    char* strerror_l(int errorNumber=errno);


    class TrSnippets {
    public:
        TrSnippets(const TrSnippets&) = delete;
        void operator=(const TrSnippets&) = delete;

        const QString enable;
        const QString shournalShellIntegration;
        const QString shournalRestore;

        static TrSnippets &instance();

    private:
        TrSnippets();
        ~TrSnippets();


        static TrSnippets* s_instance;
    };

} // namespace translation


