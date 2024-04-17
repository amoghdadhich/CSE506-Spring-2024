#include "user/user.h"


#ifdef DEBUG
    void debug(char* msg, ...) 
    {
        va_list ap;
        va_start(ap, msg);
        vprintf(1, msg, ap);
    }
#else
    void debug(char* msg, ...) { return; }
#endif