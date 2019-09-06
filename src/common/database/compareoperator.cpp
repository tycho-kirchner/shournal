#include <QHash>
#include <cassert>

#include "compareoperator.h"
#include "exccommon.h"

namespace  {

const QHash<QString, E_CompareOperator>& termEnumHash(){
    static const QHash<QString, E_CompareOperator> termEnumHash = {
        {"-gt", E_CompareOperator::GT},
        {"-ge", E_CompareOperator::GE},
        {"-lt", E_CompareOperator::LT},
        {"-le", E_CompareOperator::LE},
        {"-eq", E_CompareOperator::EQ},
        {"-ne", E_CompareOperator::NE},
        {"-like", E_CompareOperator::LIKE},
        {"-between", E_CompareOperator::BETWEEN}
    };
    return termEnumHash;
}

} // namespace



CompareOperator::CompareOperator(E_CompareOperator op) : m_operator(op) {}

/// Transform one of the commandline-passed operators into the enum and store it.
bool CompareOperator::fromTerminal(const QString &val){    
    if(val.isEmpty()){
        return false;
    }

    assert(val.at(0) == '-');

    const auto enumIt = termEnumHash().find(val);
    if(enumIt == termEnumHash().constEnd()){
        return false;
    }
    m_operator = enumIt.value();
    return true;
}

QString CompareOperator::asSql() const
{
    QString sqlOperator;
    switch (m_operator) {
    case E_CompareOperator::GT: sqlOperator = ">"; break;
    case E_CompareOperator::GE: sqlOperator = ">="; break;
    case E_CompareOperator::LT: sqlOperator = "<"; break;
    case E_CompareOperator::LE: sqlOperator = "<="; break;
    case E_CompareOperator::EQ: sqlOperator = "="; break;
    case E_CompareOperator::NE: sqlOperator = "!="; break;
    case E_CompareOperator::LIKE: sqlOperator = " LIKE "; break;
    case E_CompareOperator::BETWEEN: sqlOperator = " BETWEEN "; break;
    }
    return sqlOperator;
}

QString CompareOperator::asTerminal() const
{
    QString sqlOperator;
    switch (m_operator) {
    case E_CompareOperator::GT: sqlOperator = "-gt"; break;
    case E_CompareOperator::GE: sqlOperator = "-ge"; break;
    case E_CompareOperator::LT: sqlOperator = "-lt"; break;
    case E_CompareOperator::LE: sqlOperator = "-le"; break;
    case E_CompareOperator::EQ: sqlOperator = "-eq"; break;
    case E_CompareOperator::NE: sqlOperator = "-ne"; break;
    case E_CompareOperator::LIKE: sqlOperator = "-like"; break;
    case E_CompareOperator::BETWEEN: sqlOperator = "-between"; break;
    }
    return sqlOperator;
}

E_CompareOperator CompareOperator::asEnum() const
{
    return m_operator;
}




