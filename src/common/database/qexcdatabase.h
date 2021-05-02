
#include <QSqlError>

#include "exccommon.h"


class QExcDatabase : public QExcCommon
{
public:
     QExcDatabase(const QString & preamble,
                 const QSqlError & err);
     QExcDatabase(const QString & preamble);

};
