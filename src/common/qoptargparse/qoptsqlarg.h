#pragma once

#include <QVector>

#include "qoptarg.h"
#include "database/compareoperator.h"


class QOptSqlArg : public QOptArg
{
public:
    typedef QVector<E_CompareOperator> CompareOperators;

    static const CompareOperators& cmpOpsAll();
    static const CompareOperators& cmpOpsAllButLike();
    static const CompareOperators& cmpOpsText();
    static const CompareOperators& cmpOpsEqNe();

    QOptSqlArg(const QString& shortName, const QString & name,
               const QString& description,
               const CompareOperators& supportedOperators,
               const E_CompareOperator& defaultOperator=E_CompareOperator::EQ);

    void setParsedTrigger(const QString &parsedTrigger) override;
    QString preprocessTrigger(const char* str) const override;

    QString description() const override;

    E_CompareOperator parsedOperator() const;

private:
    E_CompareOperator m_parsedOperator;
    CompareOperators m_supportedOperators;
};

