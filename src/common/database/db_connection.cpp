
#include <cassert>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QSqlDriver>
#include <QFileInfo>
#include <QDir>
#include <unistd.h>

#include "db_connection.h"
#include "sqlite_database_scheme.h"
#include "sqlite_database_scheme_updates.h"
#include "qexcdatabase.h"
#include "qsqlquerythrow.h"
#include "logger.h"
#include "app.h"
#include "util.h"
#include "staticinitializer.h"

static QSqlDatabase* g_db = nullptr;

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

// called in case database- and application version is different
static void handleDifferentVersions(const QVersionNumber& dbVersion,
                                    QSqlQueryThrow& query){
    assert(dbVersion != app::version());

    if(dbVersion > app::version()){
        logWarning << qtr("The database version (%1) is higher than the application version (%2). "
                          "Note that downgrades of the database "
                          "are *not* supported, so things may go wrong. Please update shournal "
                          "(on this machine).")
                      .arg(dbVersion.toString()).arg(app::version().toString());
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
    query.addBindValue(app::version().toString());
    query.exec();

}

/// @param versionAction: if true, update version, if needed
/// @throws QExcDatabase
static void openAndPrepareSqliteDb()
{
    const QString appDataLoc = db_connection::mkDbPath();
    const QString dbPath = appDataLoc + "/database.db";

    bool pathExisted = QFileInfo::exists(dbPath);
    g_db->setDatabaseName(dbPath);
    if(! g_db->open()) {
        throw QExcDatabase(__func__, g_db->lastError());
    }

    QSqlQueryThrow query(*g_db);
    // Allow for delete queries with cascades

    // quoting sqlite.org/foreignkeys.html
    // "It is not possible to enable or disable foreign key constraints in the
    //  middle of a multi-statement transaction (when SQLite is not in autocommit mode)"
    // The scheme-updates require foreign_keys=OFF, so enable below
    query.exec("PRAGMA foreign_keys=OFF");

    query.transaction();

    if(! pathExisted){
        // Initially create the database
        logInfo << qtr("Creating new sqlite database");
        QStringList statements = QString(SQLITE_DATABASE_SCHEME).split(';', QString::SkipEmptyParts);

        for(const QString& stmt : statements){
            query.exec(stmt);
        }
        QFile dbfile(appDataLoc);
        if(! dbfile.setPermissions(
                    QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)){
            logWarning << qtr("Failed to initially set permissions on the database-directory "
                              "at %1: %2. Other users might be able "
                              "to browse your command history...")
                          .arg(dbPath, dbfile.errorString());
        }
    }
    const auto dbVersion = queryVersion(query);
    logDebug << "current db-version" << dbVersion.toString();
    if(dbVersion != app::version()){
        handleDifferentVersions(dbVersion, query);
    }

    query.commit();
    // outside of transaction (see above):
    query.exec("PRAGMA foreign_keys=ON");
}




const QString& db_connection::getDatabaseDir(){
    static const QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    return path;
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
