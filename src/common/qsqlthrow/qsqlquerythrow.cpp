#include <QMap>
#include <QVariant>
#include <QHash>
#include <cassert>

#include "qsqlquerythrow.h"

#include "qexcdatabase.h"
#include "util.h"


//
// QSqlQueryThrow::QSqlQueryThrow(QSqlResult *r)
//     :QSqlQuery (r),
//       m_execWasCalled(false)
// {}
//
// QSqlQueryThrow::QSqlQueryThrow(const QString &query, const QSqlDatabase& db)
//     :QSqlQuery(query, db),
//       m_execWasCalled(false)
// {}
//

static QString mkInsertIgnorePreamble(const QString& driverName)
{
    static const QHash<QString, QString> preambles {
        {"QSQLITE", "insert or ignore"}
    };
    auto it = preambles.find(driverName);
    if(it != preambles.end()){
        return it.value();
    }
    return "insert ignore";
}


QSqlQueryThrow::QSqlQueryThrow(const QSqlDatabase& db)
    : QSqlQuery (db),
      m_insertIgnorePreamble(mkInsertIgnorePreamble(db.driverName())),
      m_execWasCalled(false),
      m_withinTransaction(false)
{}

QSqlQueryThrow::~QSqlQueryThrow()
{
    if(! m_withinTransaction){
        return;
    }
    try {
        if (std::uncaught_exception()) {
            this->rollback();
        } else {
            this->commit();
        }
    } catch (const std::exception& e) {
        fprintf(stderr, "%s: %s\n", __func__, e.what());
    }

}

void QSqlQueryThrow::exec()
{
    if(! QSqlQuery::exec()){
        throw QExcDatabase(generateExcMsgExec(this->lastQuery()), this->lastError());
    }
    m_execWasCalled = true;
}

void QSqlQueryThrow::exec(const QString &query)
{
    if(! QSqlQuery::exec(query)){
        throw QExcDatabase(generateExcMsgExec(query), this->lastError());
    }
    m_execWasCalled = true;
}

void QSqlQueryThrow::prepare(const QString &query)
{
    if(! QSqlQuery::prepare(query)){
        throw QExcDatabase(qtr("prepare <%1> failed").arg(query), this->lastError());
    }
    m_execWasCalled = false;
}

bool QSqlQueryThrow::next(bool throwIfEmpty)
{
    if(! m_execWasCalled){
        throw QExcDatabase(QString("%1 was called without previous exec ")
                           .arg(__func__));
    }
    bool ret = QSqlQuery::next();
    if(throwIfEmpty && ! ret){
         throw QExcDatabase(qtr("The query %1 was expected to have (another) result which is "
                                "not the case").arg(this->lastQuery()));
    }
    return ret;
}

void QSqlQueryThrow::addBindValues(const QVariantList &vals)
{
    for(const auto& val : vals){
        this->addBindValue(val);
    }
}


/// Note: while qt's QSqlDatabase starts transactions in SQLITE in 'deferred' mode,
/// we rather choose 'immediate'. See also
/// https://www.sqlite.org/lang_transaction.html
/// and
/// https://stackoverflow.com/a/1063768
/// for the rationale.
void QSqlQueryThrow::transaction()
{
    assert(! m_withinTransaction);
    this->exec("BEGIN IMMEDIATE");
    m_withinTransaction = true;
}

void QSqlQueryThrow::commit()
{
    assert(m_withinTransaction);
    m_withinTransaction = false;
    this->exec("COMMIT");
}

void QSqlQueryThrow::rollback()
{
    assert(m_withinTransaction);
    m_withinTransaction = false;
    this->exec("ROLLBACK");
}

/// Insert values in a sql table, if these values do not already exist.
/// Requires an existing column named `id`.
/// If the value-combination does not exist, the insert-operation inserts
/// these values.
/// @param tablename: obvious.
/// @param colValPairs: pairs of column-names and corresponding values which are used for
///                     the select- and possibly insert-operation.
/// @return: the existing or newly created id
QVariant
QSqlQueryThrow::insertIfNotExist(const QString &tablename,
                                 const QSqlQueryThrow::ColnameValuePairs &colValPairs,
                                 bool *existed)
{
    QString query = "select id from " + tablename + " where ";
    int preambleSize = query.size();
    for(const auto& pair : colValPairs){
        if(query.size() != preambleSize){
            query += " and ";
        }
        query += pair.first;
        if(pair.second.isNull()){
            query += " is null ";
        } else {
            query += "=? ";
        }
    }

    this->prepare(query);
    for(const auto& pair : colValPairs){
        if(! pair.second.isNull()){
            this->addBindValue(pair.second);
        }
    }
    this->exec();
    bool nextSuccess = this->next();

    if(existed != nullptr){
        *existed = nextSuccess;
    }
    if(nextSuccess){
        return this->value(0);
    }

    // record did not exist, insert it
    query = "insert into " + tablename + " (";
    preambleSize = query.size();
    QString questionMarks('(');
    for(const auto& pair : colValPairs){
        if(query.size() != preambleSize){
            query += ',';
            questionMarks += ',';
        }
        query += pair.first;
        questionMarks += '?';

    }
    query += ')';
    questionMarks += ')';

    query += " values " + questionMarks;

    this->prepare(query);

    for(const auto& pair : colValPairs){
        this->addBindValue(pair.second);
    }
    this->exec();
    return this->lastInsertId();
}

QString QSqlQueryThrow::generateExcMsgExec(const QString &queryStr)
{
    QStringList vals;
    for(const auto& entry : this->boundValues()){
        vals.push_back(entry.value<QVariant>().toString());
    }

    QString valStr;
    if(! vals.isEmpty()){
        valStr = " with values <" + vals.join(", ") + ">";
    }

    QString msg = "exec <" + queryStr + ">" + valStr + " failed";
    return msg;
}

const QString &QSqlQueryThrow::insertIgnorePreamble() const
{
    return m_insertIgnorePreamble;
}
