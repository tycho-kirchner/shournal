
#pragma once

#include <QVariant>
#include <QVector>


class QSqlQueryThrow;

namespace db_controller {

/// Insert values in a sql table, if these values do not already exist.
/// Requires an existing column named `id`.
/// If the value-combination does not exist, the insert-operation inserts
/// these values.
class InsertIfNotExist {
public:

    InsertIfNotExist(QSqlQueryThrow& parentQuery,
                     const QString& tablename);

    void addSimple(const QString& colname, const QVariant& value);
    void addEntry(const QString& colname, const QVariantList& values,
                  const QString& placeholder);

    QVariant exec(bool* existed=nullptr);

private:
    struct InsertIfNotExistEntry {
         QString colname;
         QString placeholder;
    };

    QSqlQueryThrow& m_query;
    QString m_tablename;
    QVector<InsertIfNotExistEntry> m_entries;
    QVariantList m_values;
};


}
