#pragma once

#include <QString>
#include "util.h"

namespace db_controller {

class QueryColumns {
public:
    static QueryColumns& instance() {
        static QueryColumns s_instance;
        return s_instance;
    }

    const QString cmd_id {"cmd.id"};
    const QString cmd_txt {"cmd.txt"};
    const QString cmd_workingDir {"cmd.workingDirectory"};
    const QString cmd_comment {"cmd.comment"};
    const QString cmd_endtime {"cmd.endTime"};
    const QString cmd_starttime {"cmd.startTime"};
    const QString cmd_hashmetaId {"cmd.hashmetaId"};

    const QString env_hostname {"env.hostname"};
    const QString env_username {"env.username"};

    const QString rFile_name {"readFile.name"};
    const QString rFile_path {"readFile_path.path"}; // separate table, join alias
    const QString rFile_mtime {"readFile.mtime"};
    const QString rFile_size {"readFile.size"};
    const QString rFile_hash  {"readFile.hash"};
    const QString rFile_hashmetaId {"readFile.hashmetaId"};

    const QString wFile_id    {"writtenFile.id"};
    const QString wFile_name  {"writtenFile.name"};
    const QString wFile_mtime {"writtenFile.mtime"};
    const QString wFile_size  {"writtenFile.size"};
    const QString wFile_hash  {"writtenFile.hash"};
    const QString wFile_path  {"writtenFile_path.path"}; // separate table, join alias

    const QString session_id {"session.id"};
    const QString session_comment {"session.comment"};

private:
    QueryColumns() = default;

public:
    ~QueryColumns() = default;
    Q_DISABLE_COPY(QueryColumns)
    DEFAULT_MOVE(QueryColumns)

};

}
