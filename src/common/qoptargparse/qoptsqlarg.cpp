
#include <QDebug>

#include "qoptsqlarg.h"
#include "staticinitializer.h"


using TriggerDefinitions = QOptArgTrigger::TriggerEntries;

namespace  {


const QOptArgTrigger& allArgTrigger(){
    static QOptArgTrigger allArgTrigger;
    static StaticInitializer loader( [](){
        TriggerDefinitions triggerDefs;
        for(const E_CompareOperator& op : QOptSqlArg::cmpOpsAll()){
            int countOfConsumeVals;
            switch (op) {
            case E_CompareOperator::BETWEEN: countOfConsumeVals=2 ;break;
            default: countOfConsumeVals=1 ;break;
            }
            triggerDefs.insert(CompareOperator(op).asTerminal(), countOfConsumeVals );
        }
        allArgTrigger.setTrigger(triggerDefs);
    });
    return  allArgTrigger;
}

} // namespace




const QOptSqlArg::CompareOperators &QOptSqlArg::cmpOpsAll()
{
    static const QOptSqlArg::CompareOperators ops = {
        E_CompareOperator::GT,
        E_CompareOperator::GE,
        E_CompareOperator::LT,
        E_CompareOperator::LE,
        E_CompareOperator::EQ,
        E_CompareOperator::NE,
        E_CompareOperator::LIKE,
        E_CompareOperator::BETWEEN
    };
    return ops;
}

const QOptSqlArg::CompareOperators &QOptSqlArg::cmpOpsAllButLike()
{
    static const QOptSqlArg::CompareOperators ops = {
        E_CompareOperator::GT,
        E_CompareOperator::GE,
        E_CompareOperator::LT,
        E_CompareOperator::LE,
        E_CompareOperator::EQ,
        E_CompareOperator::NE,
        E_CompareOperator::BETWEEN
    };
    return ops;
}

const QOptSqlArg::CompareOperators &QOptSqlArg::cmpOpsText()
{
    static const QOptSqlArg::CompareOperators ops = {
        E_CompareOperator::EQ,
        E_CompareOperator::NE,
        E_CompareOperator::LIKE
    };
    return ops;
}

const QOptSqlArg::CompareOperators &QOptSqlArg::cmpOpsEqNe()
{
    static const QOptSqlArg::CompareOperators ops = {
        E_CompareOperator::EQ,
        E_CompareOperator::NE
    };
    return ops;
}


QOptSqlArg::QOptSqlArg(const QString &shortName,
                       const QString &name,
                       const QString &description,
                       const CompareOperators &supportedOperators,
                       const E_CompareOperator &defaultOperator) :
    QOptArg(shortName, name, description, allArgTrigger(),
             CompareOperator(defaultOperator).asTerminal()),
    m_parsedOperator(defaultOperator),
    m_supportedOperators(supportedOperators)
{
    if(supportedOperators.empty()){
        throw QExcIllegalArgument("supportedOperators is empty");
    }
    if(! supportedOperators.contains(defaultOperator)){
        throw QExcIllegalArgument("supportedOperators does not contain defaultOperator");
    }
}

void QOptSqlArg::setParsedTrigger(const QString &parsedTrigger)
{
    QOptArg::setParsedTrigger(parsedTrigger);
    CompareOperator op = CompareOperator();
    if(! op.fromTerminal(parsedTrigger)){
        throw ExcOptArgParse(qtr("Failed to convert %1 to a sql comparison operator")
                             .arg(parsedTrigger));
    }
    if(! m_supportedOperators.contains(op.asEnum())){
        throw ExcOptArgParse(qtr("%1 is not a supported sql comparison operator for %2")
                             .arg(parsedTrigger, name()));
    }
    m_parsedOperator = op.asEnum();
}

/// All sql parameters are processed internally as lower strings.
QString QOptSqlArg::preprocessTrigger(const char *str) const
{
    return QString(str).toLower();
}

QString QOptSqlArg::description() const
{
    QStringList operators;
    for(const auto& op : m_supportedOperators){
        operators.push_back(CompareOperator(op).asTerminal());
    }

    return m_description +
            qtr(" Supported operators: %1. Default operator: %2")
                .arg(operators.join(", "), m_defaultTriggerStr);
}

E_CompareOperator QOptSqlArg::parsedOperator() const
{
    return m_parsedOperator;
}
