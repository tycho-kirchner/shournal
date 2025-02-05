
#include <cassert>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QSqlDriver>
#include <QFileInfo>
#include <QDir>
#include <unistd.h>

#include "compat.h"
#include "db_connection.h"
#include "cflock.h"
#include "sqlite_database_scheme.h"
#include "sqlite_database_scheme_updates.h"
#include "qexcdatabase.h"
#include "qfilethrow.h"
#include "qsqlquerythrow.h"
#include "logger.h"
#include "app.h"
#include "util.h"
#include "staticinitializer.h"
#include "settings.h"

static QSqlDatabase* g_db = nullptr;

static bool versionTableExists(QSqlQueryThrow& query){
    logDebug << "checking for version table...";
    query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='version'");
    return query.next();
}

static QVersionNumber queryVersion(QSqlQueryThrow& query){
    query.exec("select ver from version");
    query.next(true);
    return QVersionNumber::fromString(query.value(0).toString());
}

static void newSqliteDbIfNeeded(){
    static StaticInitializer loader( [](){
        // maybe_todo: according to documentation of QSqlDatabase, rather
        // call QSqlDatabase::database() instead of storing the database
        // ourselves.
        g_db = new QSqlDatabase(QSqlDatabase::addDatabase("QSQLITE"));
        if(! g_db->isValid()){
            throw QExcDatabase(qtr("Failed to add qt's sqlite database driver. "
                                   "Is the driver installed?"));
        }
        // give enough time, e.g. for cases where the db is stored on a nfs-drive.
        g_db->setConnectOptions("QSQLITE_BUSY_TIMEOUT=15000");
    });
}

static void updateDbScheme(QSqlQueryThrow& query){
    const auto dbVersion = queryVersion(query);
    // Until shournal v3.2 the database version was always set to the application version.
    // This required a synchronized update of all machines sharing the same databse.
    // Therefore, only update the database version if a scheme update is necessary.
    auto latestSchemeVer = QVersionNumber{3, 2};
    if(dbVersion == latestSchemeVer){
        return;
    }
    if(dbVersion > latestSchemeVer){
        logWarning << qtr("The database version (%1) is higher than the scheme version "
                          "(%2). Note that downgrades of the database are *not* "
                          "supported, so things may go wrong. Please update shournal "
                          "(on this machine).")
                      .arg(dbVersion.toString()).arg(latestSchemeVer.toString());
        return;
    }
    // the version is smaller -> perform all necessary updates

    if(dbVersion < QVersionNumber{0, 9}){
        logDebug << "updating db to 0.9...";
        sqlite_database_scheme_updates::v0_9(query);
    }
    if(dbVersion < QVersionNumber{2, 1}){
        logDebug << "updating db to 2.1...";
        sqlite_database_scheme_updates::v2_1(query);
    }

    if(dbVersion < QVersionNumber{2, 2}){
        logDebug << "updating db to 2.2...";
        sqlite_database_scheme_updates::v2_2(query);
    }

    if(dbVersion < QVersionNumber{2, 4}){
        logDebug << "updating db to 2.4...";
        sqlite_database_scheme_updates::v2_4(query);
    }

    if(dbVersion < QVersionNumber{2, 5}){
        logDebug << "updating db to 2.5...";
        sqlite_database_scheme_updates::v2_5(query);
    }

    query.prepare("replace into version (id, ver) values (1, ?)");
    query.addBindValue(latestSchemeVer.toString());
    query.exec();

}

static void
createOrUpDateDb(QSqlQueryThrow &query, CFlock& lock){
    logDebug << "about to lockExclusive database for scheme update...";
    // quoting sqlite.org/foreignkeys.html
    // "It is not possible to enable or disable foreign key constraints in the
    //  middle of a multi-statement transaction (when SQLite is not in autocommit mode)"
    // The scheme-updates require foreign_keys=OFF, so call below pragma:
    query.exec("PRAGMA foreign_keys=OFF");
    lock.lockExclusive();
    query.transaction();

    if(! versionTableExists(query)){
        logInfo << qtr("Creating new sqlite database");
        QStringList statements = QString(
                    SQLITE_DATABASE_SCHEME).split(';', Qt::SkipEmptyParts);
        for(const QString& stmt : statements){
            query.exec(stmt);
        }

        QFile dbDir(db_connection::getDatabaseDir());
        if(! dbDir.setPermissions(
             QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner)){
            logWarning << qtr("Failed to initially set permissions on the database-"
                              "directory at %1: %2. Other users might be able "
                              "to browse your command history...")
                          .arg(db_connection::getDatabaseDir(), dbDir.errorString());
        }
    }
    updateDbScheme(query);

    query.commit();
    // outside of transaction (see above):
    // Allow for delete queries with cascades
    query.exec("PRAGMA foreign_keys=ON");
}

/// @throws QExcDatabase
static void openAndPrepareSqliteDb()
{
    const QString appDataLoc = db_connection::mkDbPath();
    const QString dbPath = appDataLoc + "/database.db";
    QFileThrow lockfile(appDataLoc + "/.shournal-dblock");
    lockfile.open(QFile::OpenModeFlag::ReadWrite);
    // Lock to allow for concurrent database scheme updates. First, we lock shared,
    // upgrading to exclusive on scheme update. Note that for some reason concurrent
    // processes executing "PRAGMA locking_mode=EXCLUSIVE; BEGIN EXCLUSIVE;" deadlocked
    // during integration tests, so be careful with that directive.
    CFlock lock(lockfile.handle());
    lock.lockShared();
    g_db->setDatabaseName(dbPath);
    if(! g_db->open()) {
        throw QExcDatabase(__func__, g_db->lastError());
    }

    QSqlQueryThrow query(*g_db);
    if(! versionTableExists(query)){
        logDebug << "version table did not exist yet..";
        createOrUpDateDb(query, lock);
    } else {
        const auto dbVersion = queryVersion(query);
        logDebug << "current db-version" << dbVersion.toString();
        if(dbVersion != app::version()){
            createOrUpDateDb(query, lock);
        }
    }
    lock.unlock();

    // Allow for delete queries with cascades
    query.exec("PRAGMA foreign_keys=ON");
}




QString db_connection::getDatabaseDir(){
    auto & sets = Settings::instance();
    return sets.dataDir();
}

/// @return the created dir
QString db_connection::mkDbPath()
{
    const QString & appDataLoc = db_connection::getDatabaseDir();
    QDir d(appDataLoc);
    if( ! d.mkpath(appDataLoc)){
        throw QExcIo(qtr("Failed to the create directory for the database at %1")
                             .arg(appDataLoc));
    }
    return  appDataLoc;
}



QueryPtr db_connection::mkQuery()
{
    setupIfNeeded();
    return std::make_shared<QSqlQueryThrow>(*g_db);
}

/// merely for test purposes
void db_connection::close()
{
    g_db->close();
}

void db_connection::setupIfNeeded()
{
    newSqliteDbIfNeeded();
    if(! g_db->isOpen()){
        openAndPrepareSqliteDb();
    }
}
