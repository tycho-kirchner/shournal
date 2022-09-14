
#include <QDateTime>
#include <QDebug>
#include <vector>

#include "db_controller.h"
#include "db_conversions.h"
#include "db_globals.h"
#include "exccommon.h"
#include "file_query_helper.h"
#include "hashcontrol.h"
#include "logger.h"
#include "os.h"
#include "qfilethrow.h"
#include "query_columns.h"
#include "settings.h"
#include "translation.h"

using std::vector;
using db_controller::QueryColumns;
using namespace db_conversions;

struct FileQueryColumns {
    FileQueryColumns(bool readFile) : readFile(readFile){ // readFile, else writtenFile
        QueryColumns& c = QueryColumns::instance();
        if(readFile){
            col_hash  = c.rFile_hash;
            col_size  = c.rFile_size;
            col_mtime = c.rFile_mtime;
            col_hashMetaId = c.rFile_hashmetaId;
        } else {
            col_hash  = c.wFile_hash;
            col_size  = c.wFile_size;
            col_mtime = c.wFile_mtime;
            col_hashMetaId = c.cmd_hashmetaId;
        }
    }

    QString col_hash;
    QString col_size;
    QString col_mtime;
    QString col_hashMetaId;
    bool readFile;
};

/// Strictly speaking a given hash for a file is
/// only valid in combination with it's hash-settings as those determine what
/// parts of a file to hash.
struct HashMetaValuePair {
    HashMeta meta;
    HashValue value;
};


static void
addToHashQuery(SqlQuery& query, const HashValue& hashVal,
               const QVariant& hashMetaId, const FileQueryColumns& c){
    // Hash-values and hashmeta are paired, so e.g.
    // (hashval=123 and hashMetaId=1) or (hashval=35 and hashMetaId=2).
    // If we query for a command, where hashing was disabled, the hash
    // naturally has be null as well.
    if(hashMetaId.isNull() && ! hashVal.isNull()){
        throw QExcProgramming("hashMetaId.isNull && ! hashVal.isNull");
    }
    SqlQuery subquery;
    subquery.addWithAnd(c.col_hash, fromHashValue(hashVal));
    subquery.addWithAnd(c.col_hashMetaId, hashMetaId);
    query.addWithOr(subquery);
}

static void
addToHashQuery(SqlQuery& query, const HashValue& hashVal,
               const HashMeta& hashMeta, const FileQueryColumns& c){
    QVariant hashMetaId;
    if(! hashMeta.isNull()){
        hashMetaId = hashMeta.idInDb;
    }
    addToHashQuery(query, hashVal, hashMetaId, c);
}

static void
addToHashQuery(SqlQuery& query, const HashMetaValuePair& p,
               const FileQueryColumns& c){
    addToHashQuery(query, p.value, p.meta, c);
}

static void
addToHashQuery(SqlQuery& query,
               const vector<HashMetaValuePair>& hashMetaValuePairs,
               const FileQueryColumns& c){
    for(const auto& p : hashMetaValuePairs){
         addToHashQuery(query, p, c);
    }
}




/// Generate all necessary HashMeta-HashValue-pairs for the given fd,
/// ignoring a possibly existing knownPair.
static vector<HashMetaValuePair>
generateHashMetaValuePairs(QFileThrow& file, qint64 filesize,
                           const FileQueryColumns* c=nullptr,
                           const HashMetaValuePair* knownPair=nullptr){
    if(filesize == 0){
        return {};
    }
    const auto hashMetas = (c==nullptr) ?
                db_controller::queryHashmetas() :
                db_controller::queryHashmetas(filesize, c->readFile);

    HashControl hashCtrl;
    vector<HashMetaValuePair> pairs;
    for(const auto& hashMeta : hashMetas){
        HashValue hashVal;
        if(! hashMeta.isNull()){
            if(knownPair != nullptr && hashMeta.idInDb == knownPair->meta.idInDb){
                // already got this one.
                continue;
            }
            hashVal = hashCtrl.genPartlyHash(file.handle(), filesize, hashMeta);
            if(hashVal.isNull()){
                throw QExcIo(qtr("file %1 - failed to hash, although it "
                                  "was not empty.").arg(file.fileName()));
            }
        }
        pairs.push_back({hashMeta, hashVal});
    }
    return pairs;
}

static bool
entriesExists(const SqlQuery& query){
    return db_controller::queryForCmd(query)->next();
}



/// Typically, changing hash-settings is a rare event,
/// so optimistically generate a hash using current settings (if
/// enabled)
static HashMetaValuePair
goodLuckHashAttempt(QFileThrow& file, const qint64 size){
    if(size == 0){
        return {};
    }
    const auto &sets = Settings::instance();
    auto hashMeta = sets.hashSettings().hashMeta;
    HashValue hashVal;
    HashControl hashCtrl;
    if(! sets.hashSettings().hashEnable || hashMeta.isNull()){
        return {};
    }
    hashMeta.idInDb = db_controller::queryHashmetaId(hashMeta);
    if(hashMeta.idInDb == db::INVALID_INT_ID){
        logDebug << "unusual event: hashmeta settings not found in db";
        return {};
    }

    hashVal = hashCtrl.genPartlyHash(file.handle(), size, hashMeta);
    if(hashVal.isNull()){
        // no need to print a warning here - is caught in
        // generateHashMetaValuePairs
        return {};
    }
    HashMetaValuePair p;
    p.meta = hashMeta;
    p.value = hashVal;
    return p;
}


/// Build a database-query based on the following file-attributes, which are collected
/// automatically: size, hash and mtime. The query attempts to be "smart", by
/// preferably returning more likely matches, lowering the strictness if nothing
/// is found.
/// It is looked preliminarily, if a file exactly matching the specs and using *only*
/// the current hash-settings exists. If nothing is found, other hash-settings (if any)
/// are used to calculate the other hashes.
/// If no entry was found, the query is set to ignore the mtime.
/// Empty files (size==0) are a special case: their hash is always null, so we can
/// ommit the hashing altogether. Over the time quite a number of empty files
/// may exist, so in this case we do **not** ignore the mtime and return 100k results.
/// In legacy shournal versions, for written files an mtime
/// not later than the file's current
/// mtime was set in the assumption that changing the mtime afterwards should
/// only increase it. However, for example wget uses the
/// «Last-Modified header» for HTTP if available,
/// which is naturally older than the system's mtime of the just downloaded
/// file.
/// @param filename: existing file, where attributes are collected from
/// @param readFile: if true, query for read files, else for written files
SqlQuery
file_query_helper::buildFileQuerySmart(const QString &filename, bool readFile)
{
    FileQueryColumns c(readFile);
    QFileThrow file(filename);
    file.open(QFile::OpenModeFlag::ReadOnly);
    auto st_ = os::fstat(file.handle());
    const QVariant mtimeVar = fromMtime(st_.st_mtime);
    const qint64 size = st_.st_size;

    if(size == 0){
        SqlQuery query;
        query.addWithAnd(c.col_size, size);
        query.addWithAnd(c.col_mtime, mtimeVar);
        return query;
    }

    vector<HashMetaValuePair> hashMetaValuePairs;
    auto firstHashRes = goodLuckHashAttempt(file, size);
    if(! firstHashRes.meta.isNull()){
        SqlQuery query;
        addToHashQuery(query, firstHashRes, c);
        query.addWithAnd(c.col_size, size);
        query.addWithAnd(c.col_mtime, mtimeVar);
        if(entriesExists(query)){
            return query;
        }
    }
    // Our goodluck first attempt failed (bad size, hash or mtime) - now query based
    // on all other hashMetaValuePairs
    hashMetaValuePairs = generateHashMetaValuePairs(file, size, &c, &firstHashRes);
    if(firstHashRes.meta.isNull() && hashMetaValuePairs.empty()){
        logDebug << filename << "no file with matching size exists";
        return mkInertSqlQuery();
    }

    if(! hashMetaValuePairs.empty()){
        SqlQuery query;
        addToHashQuery(query, hashMetaValuePairs, c);
        query.addWithAnd(c.col_size, size);
        query.addWithAnd(c.col_mtime, mtimeVar);
        if(entriesExists(query)){
            return query;
        }
    }
    // We failed to find a match with exact mtime, so, finally, perform the
    // query ignoring the mtime. Note that our first hash-result (if any)
    // is not part of hashMetaValuePairs yet.
    if(! firstHashRes.meta.isNull()){
        hashMetaValuePairs.push_back(firstHashRes);
    }
    logDebug << "will perform query on" << filename << "ignoring mtime";
    SqlQuery query;
    addToHashQuery(query, hashMetaValuePairs, c);
    query.addWithAnd(c.col_size, size);
    return query;
}


/// Build a database-query based on the following file-attributes, which are collected
/// automatically: size, hash or mtime. Any combination of those can be used.
/// @param filename: existing file, where attributes are collected from
/// @param readFile: if true, query for read files, else for written file
SqlQuery file_query_helper::buildFileQuery(const QString &filename,
                                   bool readFile,
                                   bool use_mtime, bool use_hash, bool use_size)
{
    FileQueryColumns c(readFile);
    SqlQuery query;

    QFileThrow file(filename);
    file.open(QFile::OpenModeFlag::ReadOnly);
    const auto st_ = os::fstat(file.handle());
    const QVariant mtimeVar = fromMtime(st_.st_mtime);
    const qint64 size = st_.st_size;

    if(use_mtime) query.addWithAnd(c.col_mtime, mtimeVar);
    if(use_hash){
        if(size == 0){
            if(! use_mtime && ! use_size){
                logWarning << qtr("File %1 is empty, so hash-only queries are "
                                  "not possible.").arg(filename);
                return mkInertSqlQuery();
            }
        } else {
            auto hashMetaValuePairs = generateHashMetaValuePairs(
                        file, use_size,
                        (use_size)? &c : nullptr);
            SqlQuery hashQuery;
            addToHashQuery(hashQuery, hashMetaValuePairs, c);
            query.addWithAnd(hashQuery);
        }
    }
    if(use_size) query.addWithAnd(c.col_size, qint64(size));
    return query;
}
