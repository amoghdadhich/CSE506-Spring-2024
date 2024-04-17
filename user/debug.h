#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdarg.h>
void vprintf(int fd, const char *fmt, va_list ap);
void fprintf(int, const char*, ...);

#ifdef DEBUG
    void debug(char* msg, ...) 
    {
        fprintf(2, "%s %d <%s>: ", __FILE__, __LINE__, __FUNCTION__);
        va_list ap;
        va_start(ap, msg);
        vprintf(2, msg, ap);
    }
#else
    void debug(char* msg, ...) { return; }
#endif

#endif