#pragma once

#include <QString>
#include <QObject>
#include <QVector>


enum class E_CompareOperator { GT,GE,LT,LE,EQ,NE,LIKE,BETWEEN,ENUM_END };


/// The most important sql-operators which are used
/// in this program as user-input (LE for less or equal),
/// enum and sql operator (<=)
class CompareOperator
{
public:
    CompareOperator() = default;
    CompareOperator(E_CompareOperator op);

    bool fromTerminal(const QString& val);

    QString asSql() const;
    QString asTerminal() const;
    E_CompareOperator asEnum() const;

private:
    E_CompareOperator m_operator{E_CompareOperator::EQ};
};

