
#include "shournalk_global.h"

#include "kpathtree.h"


struct kpathtree g_dummy_pathtree;

long shournalk_global_constructor(void){
    memset(&g_dummy_pathtree, 0, sizeof (struct kpathtree));
    kpathtree_init(&g_dummy_pathtree);    
    return 0;
}

void shournalk_global_destructor(void){
    kpathtree_cleanup(&g_dummy_pathtree);
}
