
#pragma once

#include "shournalk_global.h"

#include <linux/hashtable.h>

#define KFILEEXT_BITS 6

struct file_extensions {
    DECLARE_HASHTABLE(table, KFILEEXT_BITS);
    size_t n_ext; /* number of extensions within the table */
};

void file_extensions_init(struct file_extensions*);
void file_extensions_cleanup(struct file_extensions*);

long file_extensions_add(struct file_extensions* extensions,
                         const char* ext, size_t ext_len);
long file_extensions_add_multiple(struct file_extensions* extensions,
                        const char* ext_strs, size_t str_len);

bool file_extensions_contain(struct file_extensions* extensions,
                             const char* path, size_t path_len);

