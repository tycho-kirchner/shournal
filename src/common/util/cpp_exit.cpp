#include "cpp_exit.h"


/// To allow for destructor calling of local objects, throw instead of
/// calling exit
void cpp_exit(int ret)
{
    throw ExcCppExit(ret);
}
