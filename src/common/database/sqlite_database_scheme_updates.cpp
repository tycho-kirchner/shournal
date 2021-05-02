#include "sqlite_database_scheme_updates.h"


void sqlite_database_scheme_updates::v0_9(QSqlQueryThrow &query)
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


void sqlite_database_scheme_updates::v2_1(QSqlQueryThrow &query)
{
    // Add support for read files without belonging scripts.
    // Also start hashing read files as well. Because the same read file
    // can refer to multiple commands (many-to-many), it would be wrong to
    // refererence the hashMetaId of a command -> add hashmetaId column.
    query.exec("alter table `readFile` add column `hash` BLOB");
    query.exec("alter table `readFile` add column `hashmetaId` INTEGER");
    query.exec("alter table `readFile` add column `isStoredToDisk` INTEGER DEFAULT 1");

}



void sqlite_database_scheme_updates::v2_2(QSqlQueryThrow &query)
{
    // Create indeces to improve query and delete performance.
    query.exec("CREATE INDEX IF NOT EXISTS `idx_writtenFile_cmdId` ON `writtenFile` (`cmdId`)");
    query.exec("CREATE INDEX IF NOT EXISTS `idx_readFileCmd_cmdId` ON `readFileCmd` (`cmdId`)");
    query.exec("CREATE INDEX IF NOT EXISTS `idx_readFileCmd_readFileId` ON `readFileCmd` (`readFileId`)");
    query.exec("CREATE INDEX IF NOT EXISTS `idx_cmd_envId` ON `cmd` (`envId`)");
    query.exec("CREATE INDEX IF NOT EXISTS `idx_cmd_sessionId` ON `cmd` (`sessionId`)");
    query.exec("CREATE INDEX IF NOT EXISTS `idx_cmd_hashmetaId` ON `cmd` (`hashmetaId`)");
    query.exec("CREATE INDEX IF NOT EXISTS `idx_readFile_envId` ON `readFile` (`envId`)");
}


void sqlite_database_scheme_updates::v2_4(QSqlQueryThrow& query){
    // Create unified, deduplicated paths for read- and written file-events
    // Ideally we would rename path to pathId and add a foreign key to it,
    // but this is not possible with current (e.g. 3.22.0) sqlite-versions.
    // So we recreate the tables writtenFile and readFile

    query.exec(R"SOMERANDOMTEXT(
        create table if not exists `pathtable` (
           `id` INTEGER,
           `path` TEXT NOT NULL,
            PRIMARY KEY(`id`)
        )
    )SOMERANDOMTEXT"
    );
    query.exec("create unique index if not exists "
               " idx_unq_pathtable_path on pathtable (path)");

    query.exec("insert or ignore into pathtable (path) "
               "select path from writtenFile");

    query.exec (
    R"SOMERANDOMTEXT(
    CREATE TABLE `writtenFile_TMP` (
        `id`	INTEGER,
        `name`	TEXT NOT NULL,
        `pathId` INTEGER NOT NULL, /* pathId instead of path */
        `cmdId`	INTEGER NOT NULL,
        `mtime`	timestamp NOT NULL,
        `size`	INTEGER NOT NULL,
        `hash`	BLOB,
        PRIMARY KEY(`id`),
        FOREIGN KEY(`cmdId`) REFERENCES `cmd`(`id`) ON DELETE CASCADE,
        FOREIGN KEY(`pathId`) REFERENCES `pathtable`(`id`) /* new */
    ))SOMERANDOMTEXT"
    );

    // copy all data, use pathId from newly created pathtable
    query.exec("insert into writtenFile_TMP "
               "(id,name,pathId,cmdId,mtime,size,hash) "
               "select id,name,"
               "(select id from pathtable where pathtable.path=writtenFile.path),"
               "cmdId,mtime,size,hash "
               "from writtenFile");

    query.exec("drop table if exists writtenFile");

    query.exec("ALTER TABLE `writtenFile_TMP` RENAME TO `writtenFile`");

    // (re-)create indices
    // Foreign keys are always recommended to be indexed:
    // (see https://www.sqlite.org/foreignkeys.html#fk_indexes )
    query.exec("create index `idx_writtenFile_cmdId` ON `writtenFile` (`cmdId`)");
    query.exec("create index `idx_writtenFile_size` ON `writtenFile` (`size`)");
    query.exec("create index `idx_writtenFile_name` ON `writtenFile` (`name`)");
    query.exec("create index `idx_writtenFile_mtime` ON `writtenFile` (`mtime`)");
    query.exec("create index `idx_writtenFile_hash` ON `writtenFile` (`hash`)");
    // new one:
    query.exec("create index `idx_writtenFile_pathId` ON `writtenFile` (`pathId`)");


    // -------- done with writtenFile

    query.exec("insert or ignore into pathtable (path) "
               "select path from readFile");
    query.exec (
    R"SOMERANDOMTEXT(
    CREATE TABLE `readFile_TMP` (
        `id`	INTEGER,
        `envId`	INTEGER NOT NULL,
        `name`	TEXT NOT NULL,
        `pathId` INTEGER NOT NULL, /* pathId instead of path */
        `mtime`	timestamp NOT NULL,
        `size`	INTEGER NOT NULL,
        `mode`	BLOB NOT NULL,
        `hash`	BLOB,
        `hashmetaId`	INTEGER,
        `isStoredToDisk`	INTEGER DEFAULT 1,
        FOREIGN KEY(`envId`) REFERENCES `env`(`id`),
        FOREIGN KEY(`pathId`) REFERENCES `pathtable`(`id`), /* new */
        PRIMARY KEY(`id`)
    ))SOMERANDOMTEXT"
    );

    // copy all data, use pathId from newly created pathtable
    query.exec(
    R"SOMERANDOMTEXT(
    insert into readFile_TMP
    (id,envId,name,pathId,mtime,size,mode,hash,hashmetaId,isStoredToDisk)
    select id,envId,name,
    (select id from pathtable where pathtable.path=readFile.path),
    mtime,size,mode,hash,hashmetaId,isStoredToDisk
    from readFile
    )SOMERANDOMTEXT"
    );

    query.exec("drop table if exists readFile");

    query.exec("alter table `readFile_TMP` rename to `readFile`");

    query.exec("create index if not exists `idx_readFile_envId` ON `readFile` (`envId`)");
    // new one:
    query.exec("create index `idx_readFile_pathId` ON `readFile` (`pathId`)");
}
