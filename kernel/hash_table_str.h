
#pragma once

#include "shournalk_global.h"
#include "kutil.h"

#include <linux/hashtable.h>

#include "xxhash_shournalk.h"


static inline u32
__hash_table_str_do_hash(const char* path, size_t path_len) {
    return xxh32(path, path_len, 0);
}

struct hash_entry_str {
     char* str;
     size_t str_len;
     struct hlist_node node ;
};


struct hash_entry_str*
hash_entry_str_create(const char* str, size_t str_len);
void hash_entry_str_free(struct hash_entry_str* entry);


/// @param _obj_ must be passed as null, result is stored there if any
#define hash_table_str_find(_name_, _obj_, _str_, _str_len_)            \
 do {                                                                   \
    struct hash_entry_str* ____tmp;                                     \
    u32 ____str_hash;                                                   \
    ____str_hash = __hash_table_str_do_hash(_str_, _str_len_);          \
    kutil_WARN_DBG((_obj_) != NULL, "(_obj_) != NULL");                 \
    hash_for_each_possible(_name_, ____tmp, node, ____str_hash)         \
        if(____tmp->str_len == (_str_len_) &&                           \
                memcmp(____tmp->str, (_str_), (_str_len_)) == 0){       \
            (_obj_) = ____tmp;                                          \
            break;                                                      \
         }                                                              \
    } while (0)



#define hash_table_str_add(_name_, _obj_)   \
    hash_add(_name_, &(_obj_)->node,        \
             __hash_table_str_do_hash((_obj_)->str, (_obj_)->str_len))


#define hash_table_str_cleanup(_name_)                                      \
    do {                                                                    \
    u32 ____bucket;                                                         \
    struct hash_entry_str* ____el;                                          \
    struct hlist_node *____temp_node;                                       \
    hash_for_each_safe((_name_), ____bucket, ____temp_node, ____el, node) { \
        hash_del(&____el->node);                                            \
        hash_entry_str_free(____el);                                        \
    }                                                                       \
    } while (0)

