/* log.h */
#ifndef LOG_H
#define LOG_H

void log_init(void);
void dbgf(const char *fmt, ...);
void die(const char *reason);

#endif
