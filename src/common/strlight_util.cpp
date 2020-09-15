#include <cassert>

#include "strlight_util.h"


/// Get a str-reference to the file-extension in the canonical
/// filename src (no trailing slashes, etc). dest must be
/// a raw Buffer!
void strlight_util::findFileExtension_raw(const StrLight &src, StrLight &dest)
{
    if(src.size() < 3 || src.back() == '.'){
        // smallest possible filname with suffx is x.y -> 3 chars
        // Last char==dot means no extension.
        dest.setRawSize(0);
        return;
    }
    assert(src.back() != '/');

    const char* srcEnd = src.constDataEnd();
    // size - 2 or pEnd - 1, because a final dot is already excluded above
    for(const char* str = srcEnd - 1; str >= src.constData(); str-- ){
        if(*str == '/'){
            // nothing found
            break;
        }
        if(*str == '.'){
            const char* extStart = str + 1;
            dest.setRawData(extStart, srcEnd - extStart + 1);
            return;
        }
    }
    // No file extension found
    dest.setRawSize(0);
    return;
}
