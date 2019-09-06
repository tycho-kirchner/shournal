
#include <QDebug>
#include "sqlquery.h"
#include "exccommon.h"
#include "util.h"



const QString &SqlQuery::query() const
{
    return m_query;
}

QString &SqlQuery::query()
{
    return m_query;
}

const QVariantList &SqlQuery::values() const
{
    return m_values;
}

void SqlQuery::clear()
{
    m_query.clear();
    m_values.clear();
    m_columnSet.clear();
    m_tablenames.clear();
    m_ascending = true;
    m_limit = NO_LIMIT;
}

bool SqlQuery::isEmpty()
{
    return m_query.isEmpty();
}

/// @overload
void SqlQuery::addWithAnd(const QString &columnName, const QVariant &value,
                          const CompareOperator &operator_)
{
    addWithAnd(columnName, { value }, QVector<CompareOperator>{operator_});
}

/// @overload
void SqlQuery::addWithAnd(const QString &columnName,
                          const QVariantList &values, const CompareOperator &operator_,
                          bool innerAND)
{
    addWithAnd(columnName, values, QVector<CompareOperator>{operator_}, innerAND);
}


/// Add the given values to the query-string and -values.
/// If a QVariant.isNull insert "is null" instead of a placeholder.
/// @param values: The number of values must match to the number of operators,
///                except for the BETWEEN-operator (see below)
/// @param operators: The comparsion operators used for each value. In case of
///                   BETWEEN, only one operator may be passed.
/// @param innerAnd: If true, connect the column-value pair with AND, else with OR
///                  (in case of BETWEEN it is ignored)
///
/// @throws QExcIllegalArgument
void SqlQuery::addWithAnd(const QString &columnName, const QVariantList &values,
                          const QVector<CompareOperator> &operators, bool innerAND)
{
    if(values.isEmpty()){
        throw QExcIllegalArgument(QString("%1: %2 must no be empty")
                                    .arg(__func__,
                                         GET_VARIABLE_NAME(values)));
    }

    auto actualOps = expandOperatorsIfNeeded(operators, values.size());

    if(! m_query.isEmpty()){
        m_query += " and ";
    }
    m_query += " ( ";

    auto valueIt = values.begin();
    auto operatorIt = actualOps.begin();

    QByteArray innerJunction;
    if(innerAND ||
            (operators.size() == 1 &&
             operators.first().asEnum() == E_CompareOperator::BETWEEN)){
        innerJunction = " and ";
    } else {
        innerJunction = " or ";
    }
    while(valueIt != values.end()){
        if(valueIt != values.begin()){
            m_query += innerJunction;
        }
        const QVariant & var = *valueIt;

        if(var.isNull()){
            // null values are only allowed for certain operators:
            QString operatorNow;
            switch (operatorIt->asEnum()) {
            case E_CompareOperator::EQ:
            case E_CompareOperator::LIKE:
                operatorNow = " is null "; break;
            case E_CompareOperator::NE:
                operatorNow = " is not null "; break;
            default:
                throw QExcIllegalArgument("null is illegal for operator " +
                                          operatorIt->asSql() + " in column " +
                                          columnName
                                          );
            }
             m_query += columnName + operatorNow ;
        } else {
            if(operatorIt->asEnum() == E_CompareOperator::BETWEEN){
                throw QExcIllegalArgument("BETWEEN passed within list with len > 1");
            }

            m_query += columnName + operatorIt->asSql() + "? ";
            m_values.push_back(var);
        }

        ++valueIt;
        ++operatorIt;
    }

    m_query += " ) ";
    addToTableCols(columnName);
}



/// If the number of operators does not match the number of values, duplicate them, so they
/// do (in that case len(operators) *must* be 1).
/// The BETWEEN operator is a special case, it is transformed into >= and <=.
QVector<CompareOperator>
SqlQuery::expandOperatorsIfNeeded(const QVector<CompareOperator> &operators,
                                                         int nValues) const
{
    if(operators.size() == nValues){
        return operators;
    }
    if(operators.size() != 1){
        throw QExcIllegalArgument(
                    QString("len(operators) %1 !=len(values) %2 but not 1")
                    .arg(operators.size(), nValues));
    }
    const CompareOperator & op = operators.first();
    if(op.asEnum() == E_CompareOperator::BETWEEN){
        if(nValues != 2){
            throw QExcIllegalArgument(QString("BETWEEN operator requires 2 values but %1"
                                              "were given").arg(nValues));
        }
        return { E_CompareOperator::GE, E_CompareOperator::LE };
    } 
    // same operator for all values
    auto newOps = QVector<CompareOperator>();
    newOps.reserve(nValues);
    for(int i=0; i < nValues; i++){
        newOps.push_back(op);
    }
    return newOps;
    

}

/// remeber that this table-column was used. If it contains a dot,
/// the part before it is interpreted as tablename, after it as column.
void SqlQuery::addToTableCols(const QString &tableCol)
{
    int dotIdx = tableCol.indexOf('.');
    if(dotIdx == -1){
        // assume column name without table name
        m_columnSet.insert(tableCol);
    } else {
        m_tablenames.insert(tableCol.left(dotIdx));
        m_columnSet.insert(tableCol.mid(dotIdx + 1));
    }
}

/// setting the query is only allowed, it no values were set (yet)
void SqlQuery::setQuery(const QString &query)
{
    if(! m_values.isEmpty()){
        throw QExcProgramming("setting query while values not empty");
    }
    m_query = query;
}

/// @return true, if the *exact* columnname was added via 'addWithAnd'
bool SqlQuery::containsColumn(const QString &col) const
{
    return m_columnSet.find(col) != m_columnSet.end();
}

bool SqlQuery::containsTablename(const QString &table) const
{
    return m_tablenames.find(table) != m_tablenames.end();
}

int SqlQuery::limit() const
{
    return m_limit;
}

/// @param limit:  NO_LIMIT means *not* to impose a limit
void SqlQuery::setLimit(int limit)
{
    m_limit = limit;
}

/// @return 'limit x '-string or space character, if NO_LIMIT is imposed
QString SqlQuery::mkLimitString() const
{
    return (m_limit == NO_LIMIT) ? " " : "limit " + QString::number(m_limit) + " ";
}

bool SqlQuery::ascending() const
{
    return m_ascending;
}

const QString &SqlQuery::ascendingStr() const
{
    static const QString ASC_STR =  "asc ";
    static const QString DESC_STR = "desc ";
    if(m_ascending) return ASC_STR;
    return DESC_STR;
}

void SqlQuery::setAscending(bool ascending)
{
    m_ascending = ascending;
}
