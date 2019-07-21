#pragma once

#include <QString>
namespace db_controller {

class QueryColumns {
public:
    static QueryColumns& instance();

    const QString cmd_id;
    const QString cmd_txt;
    const QString cmd_workingDir;
    const QString cmd_comment;
    const QString cmd_endtime;
    const QString cmd_starttime;
    const QString env_hostname;
    const QString env_username;

    const QString rFile_name;
    const QString rFile_path;
    const QString rFile_mtime;
    const QString rFile_size;

    const QString wFile_name;
    const QString wfile_mtime;
    const QString wFile_size ;
    const QString wFile_hash ;
    const QString wFile_path ;
    const QString session_id;
    const QString session_comment;

private:
    QueryColumns();
    QueryColumns(const QueryColumns&) = delete;
    QueryColumns& operator=(const QueryColumns&) = delete;
};

}
