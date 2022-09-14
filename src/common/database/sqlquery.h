#pragma once

#include <type_traits>
#include <QVector>
#include <QVariant>
#include <unordered_set>

#include "compareoperator.h"
#include "util.h"


/// Helper class to build the query part after a sql where-clause.
/// Column-value(s)-pairs can be added to the SqlQuery in a NULL-safe
/// manner. An AND-operator is only added, if necessary (query-columncount > 1).
class SqlQuery
{
public:
    static const int NO_LIMIT {-1};

    void addWithAnd(const QString& columnName, const QVariant& value,
                    const CompareOperator& operator_=CompareOperator());

    void addWithAnd(const QString& columnName, const QVariantList& values,
                    const CompareOperator& operator_=CompareOperator(), bool innerAND=false);

    void addWithAnd(const QString& columnName, const QVariantList& values,
                    const QVector<CompareOperator>& operators, bool innerAND=false);

    const QString& query() const;
    QString& query();


    const QVariantList& values() const;

    void clear();

    bool isEmpty() const;

    bool ascending() const;
    const QString& ascendingStr() const;

    void setAscending(bool ascending);

    int limit() const;
    QString mkLimitString() const;
    void setLimit(int limit);


    void setQuery(const QString &query);

    bool containsColumn(const QString& col) const;
    bool containsTablename(const QString& table) const;

private:
    QVector<CompareOperator> expandOperatorsIfNeeded(
            const QVector<CompareOperator> &operators, int nValues) const;
    void addToTableCols(const QString& tableCol);

    QString m_query;
    QVariantList m_values;
    std::unordered_set<QString> m_columnSet;
    std::unordered_set<QString> m_tablenames;
    bool m_ascending {true};
    int m_limit {NO_LIMIT};

};


