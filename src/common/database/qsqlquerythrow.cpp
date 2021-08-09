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

/// QSQLITE does not support size(), this is a workaround
/// which only works if forwardOnly is false.
int QSqlQueryThrow::computeSize()
{
    if(this->isForwardOnly()){
        throw QExcDatabase(qtr("attempted to compute size although forwardOnly "
                               "is enabled."));
    }
    // see also https://stackoverflow.com/a/26500811/7015849
    const int initialPos = this->at();    
    const int size = (this->last()) ? this->at() + 1 : 0;
    // restore initial pos
    switch (initialPos) {
    case QSql::BeforeFirstRow:
        this->first();
        this->previous();
        break;
    case QSql::AfterLastRow:
        this->last();
        this->next();
        break;
    default:
        this->seek(initialPos);
        break;
    }
    return size;

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
