#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdarg.h>
void vprintf(int fd, const char *fmt, va_list ap);
void fprintf(int, const char*, ...);

#ifdef DEBUG
#define debug(msg, ...) printf("DEBUG: %s line %d in <%s>: ", __FILE__, __LINE__, __FUNCTION__); printf(msg, ##__VA_ARGS__)
#else
#define debug(msg, ...) do{} while (0)
#endif

#endif