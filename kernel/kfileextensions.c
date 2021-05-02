

#include "kfileextensions.h"

#include "hash_table_str.h"


void file_extensions_init(struct file_extensions* extensions){
    hash_init(extensions->table);
    extensions->n_ext = 0;
}


void file_extensions_cleanup(struct file_extensions* extensions){
    hash_table_str_cleanup(extensions->table);
}


long file_extensions_add(struct file_extensions* extensions,
                         const char* ext, size_t ext_len){
    struct hash_entry_str* entry;

    // we store file extensions in the table without
    // leading dot.
    kutil_WARN_DBG(ext_len == 0, "ext_len == 0");
    kutil_WARN_DBG(ext[0] == '.', "ext[0] == '.'");
    kutil_WARN_DBG(strnstr(ext, "/", ext_len) != NULL, "strnstr(ext, /, ext_len)");

    entry =  hash_entry_str_create(ext, ext_len);
    if(IS_ERR(entry)){
        return PTR_ERR(entry);
    }
    hash_table_str_add(extensions->table, entry);
    extensions->n_ext++;
    return 0;
}


long file_extensions_add_multiple(struct file_extensions* extensions,
                        const char* ext_strs, size_t str_len){
    long ret;
    const char* end = ext_strs + str_len;
    const char* s;

    for(s = ext_strs; s < end; s++) {
        if(*s == '/'){
            size_t s_len = s - ext_strs;
            if(unlikely(s_len < 1)){
                pr_debug("empty extension passed\n");
                return -EINVAL;
            }
            if(unlikely((ret=file_extensions_add(extensions, ext_strs, s_len)))){
                return ret;
            }
            ext_strs = s + 1;
        }
    }
    if(unlikely(s != ext_strs)){
        pr_debug("extensions-string did not have trailing /\n");
        return -EINVAL;
    }
    return 0;
}



/// Check if the file-extension (if any) of the given canonical path
/// is contained within the struct file_extensions
bool file_extensions_contain(struct file_extensions* extensions,
                             const char* path, size_t path_len){
    const char* str;
    const char* const end = path + path_len - 1;

    // empty paths are not allowed here
    kutil_WARN_ON_DBG(path_len == 0);

    if(*end == '.') return false;

    // loop backwards through the string until the first slash (no extension)
    // or dot is found
    for(str = end - 1; str >= path; str-- ){
        if(*str == '/'){
            // nothing found
            break;
        }
        if(*str == '.'){
            struct hash_entry_str* entry = NULL;
            const char* ext_start = str + 1;
            size_t ext_len = end - ext_start + 1;
            hash_table_str_find(extensions->table, entry, ext_start, ext_len);
            return entry != NULL;
        }
    }
    return false;
}
