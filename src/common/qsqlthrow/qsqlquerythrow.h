#pragma once

#include <QSqlQuery>
#include <QVariant>
#include <QVector>

class QSqlQueryThrow : public QSqlQuery
{
public:
    // explicit QSqlQueryThrow(QSqlResult *r);
    // explicit QSqlQueryThrow(const QString& query = QString(), const QSqlDatabase& db = QSqlDatabase());
    explicit QSqlQueryThrow(const QSqlDatabase& db);
    ~QSqlQueryThrow();
    
    void exec();
    void exec(const QString& query);

    void prepare(const QString& query);

    bool next(bool throwIfEmpty=false);

    void addBindValues(const QVariantList& vals);

    void transaction();
    void commit();
    void rollback();

    int computeSize();

public:
    typedef QVector<QPair<const char*, QVariant> > ColnameValuePairs;

    QVariant insertIfNotExist(const QString& tablename, const ColnameValuePairs& colValPairs,
                              bool* existed=nullptr);
    const QString& insertIgnorePreamble() const;

public:
    // disable-copies: transactions cannot be copied...
    QSqlQueryThrow(const QSqlQueryThrow &) = delete ;
    void operator=(const QSqlQueryThrow &) = delete ;


private:
    QString generateExcMsgExec(const QString& queryStr);
    QString m_insertIgnorePreamble;
    bool m_execWasCalled;
    bool m_withinTransaction;
};

