
#include <QDateTime>
#include <QDebug>

#include "file_query_helper.h"
#include "query_columns.h"
#include "db_controller.h"
#include "db_conversions.h"
#include "db_globals.h"
#include "hashcontrol.h"
#include "exccommon.h"
#include "os.h"
#include "qfilethrow.h"

using db_controller::QueryColumns;
using namespace db_conversions;


namespace {


/// Generate all necessary hashes for the given fd (seek at 0!). A hash is considered
/// necessary, if a hashmeta-entry exists for the given filesize.
QVariantList generateAllNeededHashes(HashControl& hashCtrl, int fd, qint64 filesize){
    auto hashMetas = db_controller::queryHashmetas(filesize);
    QVariantList hashValues;
    for(const auto& hashMeta : hashMetas){
        HashValue hashVal;
        if(! hashMeta.isNull()){
            hashVal = hashCtrl.genPartlyHash(fd, filesize, hashMeta);
        }
        hashValues.push_back(fromHashValue(hashVal));
    }
    return hashValues;
}

} // namespace

/// Add the following file-attributes to the query, which are collected automatically:
/// size, hash and mtime. It is looked preliminarily, if a file exactly matching the specs
/// exists, in which case the passed query is filled with those (result is discarded).
/// If no entry was found, the query is set to ignore the mtime.
/// In previous shournal versions an mtime not later than the file's current mtime
/// was set in the assumption that changing the mtime afterwards should only increase it.
/// However, for example wget uses the «Last-Modified header» for HTTP if available,
/// which is naturally older than the system's mtime of the just downloaded file.
void file_query_helper::addWrittenFileSmart(SqlQuery &query, const QString &filename)
{
    QFileThrow file(filename);
    file.open(QFile::OpenModeFlag::ReadOnly);
    auto st = os::fstat(file.handle());
    const QVariant mtimeVar = fromMtime(st.st_mtime);

    HashControl hashCtrl;
    QVariantList hashValues = generateAllNeededHashes(hashCtrl, file.handle(), st.st_size);
    QueryColumns & queryCols = QueryColumns::instance();
    if( hashValues.isEmpty()){
        // actually the query could end here, because no file with matching size exists.
        // For the sake of consistency let the caller handle it.
        query.addWithAnd(queryCols.wFile_size, static_cast<qint64>(st.st_size));
        query.addWithAnd(queryCols.wfile_mtime, mtimeVar);
        return;
    }

    // At least one entry with matching size exists.
    // Try to find entries with *exactly* matching mtime
    SqlQuery mtimeExactQuery;
    mtimeExactQuery.addWithAnd(queryCols.wFile_hash, hashValues);
    mtimeExactQuery.addWithAnd(queryCols.wFile_size, static_cast<qint64>(st.st_size));
    mtimeExactQuery.addWithAnd(queryCols.wfile_mtime, mtimeVar);

    E_CompareOperator mtimeCmpOperator;
    if(! db_controller::queryForCmd(mtimeExactQuery)->next()){
        // ignore mtime
        mtimeCmpOperator = E_CompareOperator::ENUM_END;
    } else {
        mtimeCmpOperator = E_CompareOperator::EQ;
    }

    query.addWithAnd(queryCols.wFile_hash, hashValues);
    query.addWithAnd(queryCols.wFile_size, static_cast<qint64>(st.st_size));
    if(mtimeCmpOperator != E_CompareOperator::ENUM_END){
        query.addWithAnd(queryCols.wfile_mtime,
                         mtimeVar, mtimeCmpOperator);
    }

}

void file_query_helper::addWrittenFile(SqlQuery &query, const QString &filename,
                                   bool mtime, bool hash_, bool size)
{
    QFileThrow file(filename);
    file.open(QFile::OpenModeFlag::ReadOnly);
    auto st = os::fstat(file.handle());

    QueryColumns & queryCols = QueryColumns::instance();
    if(mtime) query.addWithAnd(queryCols.wfile_mtime, fromMtime(st.st_mtime));
    if(hash_){
        HashControl hashCtrl;
        QVariantList hashValues = generateAllNeededHashes(hashCtrl, file.handle(), st.st_size);
        if( ! hashValues.isEmpty()) {
            query.addWithAnd(queryCols.wFile_hash, hashValues);
        }
    }
    if(size) query.addWithAnd(queryCols.wFile_size, static_cast<qint64>(st.st_size));


}
