

#include "shournalk_test.h"

#include "kpathtree.h"
#include "hash_table_str.h"


#define TEST_FAIL_ON(condition) ({						\
    if(!!(condition)){  \
        pr_warn("test fail at %s:%d/%s\n", __FILE__, __LINE__, __func__); \
        goto test_err_out; \
    } \
})


static bool test_kpathtree(void){
    const char* p1 = "/home/user1";
    const char* p2 = "/home/user2";
    const char* p3 = "/mnt/d";
    const char** current_ppath;

    const char* subpaths[] = {
        "/home/user1/a",
        "/home/user2/a",
        "/home/user1/abc/defg",
        "/home/user2/abc/defg__long_stuff.txt.tar.gz",
        "/mnt/d/1",
        "/mnt/d/2/abc/defg__long_stuff.txt.tar.gz",
        NULL
    };

    const char* nosubpaths[] = {
        "/home/user3/a",
        "/home/user1",
        "/home/user2",
        "/home",
        "/",
        "/media/user1",
        "/mnt/data",
        "/mnt/e",
        "/mnt/defghijk/lmnop",
        NULL
    };

    struct kpathtree* t = kpathtree_create();
    TEST_FAIL_ON(IS_ERR(t));

    // special case root node (all paths (except /) are subpaths
    TEST_FAIL_ON(kpathtree_add(t, "/", 1));
    for(current_ppath = subpaths; *current_ppath != NULL; current_ppath++){
        // pr_info("current path: %s\n", *current_ppath);
        TEST_FAIL_ON(! kpathtree_is_subpath(t, *current_ppath, (int)strlen(*current_ppath),0));
    }
    kpathtree_free(t);
    t = kpathtree_create();
    TEST_FAIL_ON(IS_ERR(t));

    // before a path is added, all should fail:
    for(current_ppath = subpaths; *current_ppath != NULL; current_ppath++){
        TEST_FAIL_ON(kpathtree_is_subpath(t, *current_ppath, (int)strlen(*current_ppath),0));
    }
    for(current_ppath = nosubpaths; *current_ppath != NULL; current_ppath++){
        TEST_FAIL_ON(kpathtree_is_subpath(t, *current_ppath, (int)strlen(*current_ppath),0));
    }

    TEST_FAIL_ON(kpathtree_add(t, p1, (int)strlen(p1)));
    TEST_FAIL_ON(kpathtree_add(t, p2, (int)strlen(p2)));
    TEST_FAIL_ON(kpathtree_add(t, p3, (int)strlen(p3)));

    for(current_ppath = subpaths; *current_ppath != NULL; current_ppath++){
        // pr_info("current path: %s\n", *current_ppath);
        TEST_FAIL_ON(! kpathtree_is_subpath(t, *current_ppath, (int)strlen(*current_ppath),0));
    }

    for(current_ppath = nosubpaths; *current_ppath != NULL; current_ppath++){
        // pr_info("current path: %s\n", *current_ppath);
        TEST_FAIL_ON(kpathtree_is_subpath(t, *current_ppath, (int)strlen(*current_ppath),0));
    }

    kpathtree_free(t);
    t = kpathtree_create();
    TEST_FAIL_ON(IS_ERR(t));

    // test allow_equals
    TEST_FAIL_ON(kpathtree_add(t, p1, (int)strlen(p1)));
    TEST_FAIL_ON(kpathtree_is_subpath(t, p1,(int)strlen(p1),0));
    TEST_FAIL_ON(!kpathtree_is_subpath(t, p1,(int)strlen(p1),1));

    kpathtree_free(t);
    return true;

test_err_out:
    if(! IS_ERR(t)) kpathtree_free(t);
    return false;
}

static bool test_hash_table_str(void){
    struct hash_entry_str* orig_e = NULL;
    struct hash_entry_str* back_e = NULL;
    const char* str1 = "foobar";
    DEFINE_HASHTABLE(hash_table, 6);

    orig_e = hash_entry_str_create(str1, strlen(str1));
    TEST_FAIL_ON(IS_ERR_OR_NULL(orig_e));

    hash_table_str_add(hash_table, orig_e);
    hash_table_str_find(hash_table, back_e, str1, strlen(str1));
    TEST_FAIL_ON( back_e == NULL);

    hash_table_str_cleanup(hash_table);
    // was just freed
    orig_e = NULL;
    back_e = NULL;
    hash_table_str_find(hash_table, back_e, str1, strlen(str1));
    TEST_FAIL_ON( back_e != NULL);

    return true;

test_err_out:
    if(! IS_ERR_OR_NULL(orig_e)) hash_entry_str_free(orig_e);
    return false;
}


bool run_tests(void){
    if(! test_kpathtree()) return false;
    if(! test_hash_table_str()) return false;

    pr_devel("Version %s - Tests successful!\n", SHOURNAL_VERSION);
    return true;
}
