#include "sqlite_database_scheme_updates.h"


void sqlite_database_scheme_updates::zero_point_nine(QSqlQueryThrow &query)
{
    // until this version no scripts (read files) were stored in the database
    // so the tables can be dropped (and re-created) safely.
    // Further rename tables to better represent read/write events
    query.exec("ALTER TABLE `file` RENAME TO `writtenFile`");

    query.exec("drop index idx_exemeta_exepath");
    query.exec("drop index idx_exeFile_name");
    query.exec("drop index idx_exeFile_mtime");
    query.exec("drop index idx_exeFile_size");

    query.exec("drop table exeMeta");
    query.exec("drop table exeFile");
    query.exec("drop table exeFileCmd");

    query.exec(
        "CREATE TABLE IF NOT EXISTS `readFile` ("
          "`id` INTEGER,"
          "`envId` INTEGER NOT NULL references `env`(id),"
          "`name` TEXT NOT NULL,"
          "`path` TEXT NOT NULL,"
          "`mtime` timestamp NOT NULL,"
          "`size` INTEGER NOT NULL,"
          "`mode` BLOB NOT NULL,"
          "PRIMARY KEY(`id`)"
        ")"
    );

    query.exec(
        "CREATE TABLE IF NOT EXISTS `readFileCmd` ("
          "`id` INTEGER,"
          "`cmdId` INTEGER NOT NULL references `cmd`(id) ON DELETE CASCADE,"
          "`readFileId` INTEGER references readFile(id),"
          "PRIMARY KEY(`id`)"
        ")"
    );
}
