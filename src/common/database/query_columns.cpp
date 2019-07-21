#include "query_columns.h"



db_controller::QueryColumns::QueryColumns() :
    cmd_id("cmd.id"),
    cmd_txt("cmd.txt"),
    cmd_workingDir("cmd.workingDirectory"),
    cmd_comment("cmd.comment"),
    cmd_endtime("cmd.endTime"),
    cmd_starttime("cmd.startTime"),
    env_hostname("env.hostname"),
    env_username("env.username"),
    rFile_name("readFile.name"),
    rFile_path("readFile.path"),
    rFile_mtime("readFile.mtime"),
    rFile_size("readFile.size"),
    wFile_name("writtenFile.name"),
    wfile_mtime("writtenFile.mtime"),
    wFile_size ("writtenFile.size"),
    wFile_hash ("writtenFile.hash"),
    wFile_path ("writtenFile.path"),
    session_id("session.id"),
    session_comment("session.comment")
{

}

db_controller::QueryColumns &db_controller::QueryColumns::instance()
{
    static QueryColumns s_instance;
    return s_instance;
}
