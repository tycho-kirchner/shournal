
#include "kpathtree.h"

#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/sort.h>

#include "kutil.h"
#include "xxhash_shournalk.h"
#include "hash_table_str.h"


static int __compare_ints(const void *lhs, const void *rhs) {
    int lhs_integer = *(const int *)(lhs);
    int rhs_integer = *(const int *)(rhs);

    if (lhs_integer < rhs_integer) return -1;
    if (lhs_integer > rhs_integer) return 1;
    return 0;
}

static bool __path_len_exists(struct kpathtree* pathtree, int path_len){
    int i;
    // maybe_todo: the path sizes are sorted,
    // so this coul be improved. However, KPATHTREE_MAX_SIZE
    // is small and adding a path not performance-critical..
    for(i=0; i < pathtree->__n_path_sizes; i++){
        if(pathtree->__path_sizes[i] == path_len){
            return true;
        }
    }
    return false;
}




////////////////////////////////////////////////////////////////////

struct kpathtree* kpathtree_create(void){
    struct kpathtree* pathtree = kzalloc(sizeof (struct kpathtree), SHOURNALK_GFP);
    if(pathtree == NULL){
        return ERR_PTR(-ENOMEM);
    }
    kpathtree_init(pathtree);
    return pathtree;
}

void kpathtree_free(struct kpathtree* pathtree){
    kpathtree_cleanup(pathtree);
    kfree(pathtree);
}


/// pathtree must have been nulled before
void kpathtree_init(struct kpathtree* pathtree){
    WARN(pathtree->__is_init, "pathtree already initialized!");

    pathtree->n_paths = 0;
    pathtree->__n_path_sizes = 0;
    hash_init(pathtree->path_table);
    mutex_init(&pathtree->lock);
    pathtree->__is_init = true;
}

void kpathtree_cleanup(struct kpathtree* pathtree){
    if(! pathtree->__is_init){
        WARN(1, "pathtree not initialized!");
        return;
    }
    hash_table_str_cleanup(pathtree->path_table);

    pathtree->__is_init = false;
}


long kpathtree_add(struct kpathtree* pathtree, const char* path, int path_len){
    struct hash_entry_str* entry;

    if(pathtree->n_paths >= KPATHTREE_MAX_SIZE){
        return -ENOSPC;
    }
    entry =  hash_entry_str_create(path, path_len);
    if(IS_ERR(entry)){
        return PTR_ERR(entry);
    }
    hash_table_str_add(pathtree->path_table, entry);
    pathtree->n_paths++;

    if(! __path_len_exists(pathtree, path_len)){
        pathtree->__path_sizes[pathtree->__n_path_sizes] = path_len;
        pathtree->__n_path_sizes++;
        sort(pathtree->__path_sizes, pathtree->__n_path_sizes, sizeof(int), &__compare_ints, NULL);
    }
    return 0;
}


bool kpathtree_is_subpath(struct kpathtree* pathtree, const char* path,
                          int path_len, bool allow_equals){
    int i;
    struct hash_entry_str* entry = NULL;

    if(pathtree->n_paths == 0){
        return false;
    }
    if(pathtree->__path_sizes[0] == 1){
        // We contain the root node (if input is valid - else we don't care).
        // As this function is only intended for file-paths, just:
        return true;
    }
    for(i=0; i < pathtree->__n_path_sizes; i++){
        int s = pathtree->__path_sizes[i];

        if(s < path_len){
            // If we didn't have a / at the next position, we would cut the
            // path at a wrong position -> continue
            if(path[s] != '/'){
                continue;
            }
            // A candiate path with the same size exists.
            // maybe_todo: incremental hash (but xxhash_update also
            // has overhead..)
            hash_table_str_find(pathtree->path_table, entry, path, (size_t)s);
            if(entry != NULL){
                return true;
            }
            // keep going
        } else if(s > path_len) {
            // __path_sizes is ordered ascending -> the
            // next paths will be even longer:
            return false;
        } else {
            // s == path.size
            // The next m_orderedPathlength will be greater, so we can only
            // be a 'sub'-path, if allow_equals is true.
            if(allow_equals){
                hash_table_str_find(pathtree->path_table, entry, path, (size_t)s);
                return entry != NULL;
            }
            return false;
        }
    }
    return false;
}

