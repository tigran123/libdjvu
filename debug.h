#ifndef _DEBUG_H
#define _DEBUG_H

#define DEBUG 0

#if DEBUG
//#define DPRINTF(str, args...)   fprintf(logfp ? logfp : stderr, str, ##args)
#define DPRINTF(str, args...)   fprintf(stderr, str, ##args)
#else
#define DPRINTF(str, ...)
#endif

#if DEBUG
extern FILE *logfp;
#endif

#endif
