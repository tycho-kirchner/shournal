
#include "qexcdatabase.h"




QExcDatabase::QExcDatabase(const QString &preamble, const QSqlError &err) :
    QExcCommon (preamble)
{
    if(! descrip().isEmpty()){
        setDescrip( descrip() + ": ");
    }
    setDescrip( descrip() + err.text()
                + '('+ err.nativeErrorCode() + ')');
}

QExcDatabase::QExcDatabase(const QString &preamble) :
    QExcCommon (preamble)
{

}
