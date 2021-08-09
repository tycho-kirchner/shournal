

#include "insertifnotexist.h"

#include "qsqlquerythrow.h"

db_controller::InsertIfNotExist::InsertIfNotExist(QSqlQueryThrow &parentQuery,
                                                  const QString &tablename) :
    m_query(parentQuery),
    m_tablename(tablename)
{}


void db_controller::InsertIfNotExist::addSimple(const QString &colname, const QVariant &value)
{
    InsertIfNotExist::addEntry(colname, {value}, "?");
}

/// Add a columnname-value pair for the prospective selcet/insert queuey.
/// @param colname: Column-name.
/// @param values: value-list for QSqlQuery::addBindValue. In most cases
/// only one value is supplied, however, multiple values are possible to
/// allow for sub-queries.
/// @param placeholder: In simple cases the placeholder is ?, e.g.
/// «colname is ?», however, for sub-queries this may expand to e.g.
/// «(select id from table2 where foo is ? and bar is ?)».
void db_controller::InsertIfNotExist::
addEntry(const QString &colname, const QVariantList &values,
         const QString &placeholder)
{
    InsertIfNotExistEntry e;
    e.colname = colname;
    e.placeholder = placeholder;
    m_entries.push_back(e);
    for(const auto& val : values){
        m_values.push_back(val);
    }
}

/// Execute the insert-if-not exist query using the
/// previously added column-value-pairs.
/// @param existed: If non-null, set it to true, if
/// the entry already existed (so no insert was necessary).
/// @return: the existing or newly created id
QVariant db_controller::InsertIfNotExist::exec(bool *existed)
{
    QString query = "select id from " + m_tablename + " where ";
    bool first = true;
    for(const auto& entry : m_entries){
        if(! first){
            query += " and ";
        }
        first = false;
        query += entry.colname + " is " + entry.placeholder;
    }
    m_query.prepare(query);
    m_query.addBindValues(m_values);
    m_query.exec();

    bool nextSuccess = m_query.next();

    if(existed != nullptr){
        *existed = nextSuccess;
    }
    if(nextSuccess){
        return m_query.value(0);
    }

    // record did not exist, insert it
    query = "insert into " + m_tablename + " (";
    first = true;
    QString placeholders('(');
    for(const auto& entry : m_entries){
        if(! first){
            query += ',';
            placeholders += ',';
        }
        first = false;
        query += entry.colname;
        placeholders += entry.placeholder;
    }
    query += ')';
    placeholders += ')';

    query += " values " + placeholders;

    m_query.prepare(query);
    m_query.addBindValues(m_values);
    m_query.exec();
    return m_query.lastInsertId();
}
