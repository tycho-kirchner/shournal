#include "qresource_helper.h"
#include "compat.h"

///
/// \brief qresource_helper::data_safe uncompress data as neeeded
/// \param r
/// \return
///
QByteArray qresource_helper::data_safe(QResource &r)
{
    QByteArray data = Qt::resourceIsCompressed(r) ? qUncompress(r.data(), int(r.size())) :
         QByteArray(reinterpret_cast<const char*>(r.data()));
    return data;
}
