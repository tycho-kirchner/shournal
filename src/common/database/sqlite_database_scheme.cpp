#include "sqlite_database_scheme.h"


// Note: this is the initial scheme, please don't change it.
// To add new stuff (tables/columns/indexes) please do that in
// sqlite_database_scheme_updates.cpp
const char* DATABASE_SCHEME = R"SOMERANDOMTEXT(

CREATE TABLE IF NOT EXISTS `version` (
 `id` INTEGER PRIMARY KEY,
 `ver` TEXT NOT NULL
);


CREATE TABLE IF NOT EXISTS `env` (
 `id` INTEGER,
 `username`	TEXT NOT NULL,
 `hostname`	TEXT NOT NULL,
 PRIMARY KEY(`id`),
 CONSTRAINT unq UNIQUE (username, hostname)
);


CREATE TABLE IF NOT EXISTS `hashmeta` (
 `id` INTEGER ,
 `chunkSize` INTEGER NOT NULL,
 `maxCountOfReads` INTEGER NOT NULL,
 PRIMARY KEY(`id`),
CONSTRAINT unq UNIQUE (`chunkSize`,`maxCountOfReads`)
);

CREATE TABLE IF NOT EXISTS `cmd` (
 `id` INTEGER,
 `sessionId` BLOB references session(id),
 `envId` INTEGER NOT NULL references env(id),
 `hashmetaId` INTEGER, /* NULL-able because hash may be disabled */
 `txt` TEXT NOT NULL,
 `returnVal` INTEGER NOT NULL,
 `startTime` timestamp NOT NULL,
 `endTime` timestamp NOT NULL,
 `workingDirectory` TEXT NOT NULL,
 PRIMARY KEY(`id`)
);


CREATE TABLE IF NOT EXISTS `file` (
 `id` INTEGER,
 `path` TEXT NOT NULL,
 `name` TEXT NOT NULL,
 `cmdId` INTEGER NOT NULL references cmd(id) ON DELETE CASCADE,
 `mtime` timestamp NOT NULL,
 `size` INTEGER NOT NULL,
 `hash` BLOB, /* 64 bit unsigned int, so use blob... */
 PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `exeMeta` (
 `id` INTEGER,
 `envId` INTEGER NOT NULL references `env`(id),
 `exepath` TEXT NOT NULL,
 PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `exeFile` (
 `id` INTEGER,
 `exeMetaId` INTEGER NOT NULL references `exeMeta`(id),
 `name`  TEXT NOT NULL,
 `mtime` timestamp NOT NULL,
 `size` INTEGER NOT NULL,
 `isExecutable` bool NOT NULL,
 PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `exeFileCmd` (
 `id` INTEGER,
 `cmdId` INTEGER NOT NULL,
 `exeFileId` INTEGER references exeFile(id),
 PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `session` (
 `id`   BLOB,
 `comment` TEXT,
 PRIMARY KEY(`id`)
);

CREATE INDEX IF NOT EXISTS idx_file_name ON `file` (`name`);
CREATE INDEX IF NOT EXISTS idx_file_mtime ON `file` (`mtime`);
CREATE INDEX IF NOT EXISTS idx_file_size ON `file` (`size`);
CREATE INDEX IF NOT EXISTS idx_file_hash ON `file` (`hash`);

CREATE INDEX IF NOT EXISTS idx_exemeta_exepath ON `exeMeta` (`exepath`);
CREATE INDEX IF NOT EXISTS idx_exeFile_name ON `exeFile` (`name`);
CREATE INDEX IF NOT EXISTS idx_exeFile_mtime ON `exeFile` (`mtime`);
CREATE INDEX IF NOT EXISTS idx_exeFile_size ON `exeFile` (`size`);

replace into version (id, ver) values (1, '0.1');)SOMERANDOMTEXT";

