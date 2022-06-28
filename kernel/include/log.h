#ifndef LOG_H
#define LOG_H
#include <config.h>
extern int locked;
void lock_();
void unlock_();
#ifdef LOCAL_LOG
    #define LOG_INIT() do {\
    } while (0)

    #define LOG(fmt, ...) do {\
        for (int i = 0; i < cpu_current() * 50; i++) {\
            printf(" ");\
        }\
        printf("[%s %d][%d]: ", __func__, __LINE__, cpu_current()); \
        printf(fmt, ##__VA_ARGS__);\
    } while (0)

    #define COND_LOG(s, fmt, ...) do {\
        if (s) {\
            for (int i = 0; i < cpu_current() * 10; i++) {\
                printf(" ");\
            }\
            printf("[%s %d][%d]: ", __func__, __LINE__, cpu_current()); \
            printf(fmt, ##__VA_ARGS__);\
            panic("");\
        }\
    } while (0)

#else
    #define LOG(...) 
    #define COND_LOG(...)
    #define LOG_INIT()
#endif
#endif