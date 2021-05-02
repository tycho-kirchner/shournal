
#include "hash_table_str.h"

#include <linux/slab.h>
#include <linux/mm.h>


static const int HASH_GFP_FLAGS = SHOURNALK_GFP | __GFP_RETRY_MAYFAIL;

/// creates a copy of the passed string
struct hash_entry_str*
hash_entry_str_create(const char* str, size_t str_len){
    struct hash_entry_str* str_entry = kmalloc(sizeof (struct hash_entry_str),
                                          HASH_GFP_FLAGS);
    if(str_entry == NULL){
        return ERR_PTR(-ENOMEM);
    }
    str_entry->str = kmalloc(str_len, HASH_GFP_FLAGS);
    if(str_entry->str == NULL){
        kfree(str_entry);
        return ERR_PTR(-ENOMEM);
    }

    memcpy(str_entry->str, str, str_len);
    str_entry->str_len = str_len;
    return str_entry;
}


void hash_entry_str_free(struct hash_entry_str* entry){
    kfree(entry->str);
    kfree(entry);
}

