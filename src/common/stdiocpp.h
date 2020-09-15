#pragma once

#include "exccommon.h"

/// Very thin wrappers around some stdio functions
/// with exceptions, etc.
namespace stdiocpp {

class QExcStdio : public QExcCommon
{
public:
    explicit QExcStdio(QString text,
                       const FILE* file, bool collectErrno=false,
                       bool collectStacktrace=true);
};


FILE *fopen(const char *pathname, const char *mode);
void fclose(FILE *stream);
int fgetc_unlocked(FILE *stream);

size_t fwrite_unlocked(const void *ptr, size_t size, size_t n_items,
                              FILE *stream);
void fflush(FILE *stream);
size_t fread_unlocked(void *ptr, size_t size, size_t n,
                             FILE *stream);
int fseek(FILE *stream, long offset, int whence);
void ftruncate_unlocked(FILE* stream);

} // namespace stdiocpp




