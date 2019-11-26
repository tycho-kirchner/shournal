#include "qresource_helper.h"


///
/// \brief qresource_helper::data_safe uncompress data as neeeded
/// \param r
/// \return
///
QByteArray qresource_helper::data_safe(QResource &r)
{
    QByteArray data = (r.isCompressed()) ? qUncompress(r.data(), int(r.size())) :
         QByteArray(reinterpret_cast<const char*>(r.data()));
    return data;
}
