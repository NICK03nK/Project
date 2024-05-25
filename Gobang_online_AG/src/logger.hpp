#pragma once

#include <iostream>
#include <ctime>
#include <cstdio>

#define INF 0
#define DBG 1
#define ERR 2
#define DEFAULT_LOG_LEVEL INF

#define LOG(level, format, ...)                                                                 \
    do                                                                                          \
    {                                                                                           \
        if (DEFAULT_LOG_LEVEL > level)                                                          \
            break;                                                                              \
        time_t t = time(NULL);                                                                  \
        struct tm *lt = localtime(&t);                                                          \
        char buf[32] = {0};                                                                     \
        strftime(buf, 31, "%H:%M:%S", lt);                                                      \
        FILE* pf_log = fopen("./log.txt", "a");                                                 \
        if (pf_log)                                                                             \
        {                                                                                       \
            fprintf(pf_log, "[%s %s:%d] " format "\n", buf, __FILE__, __LINE__, ##__VA_ARGS__); \
            fclose(pf_log);                                                                     \
        }                                                                                       \
        else                                                                                    \
        {                                                                                       \
            fprintf(stderr, "Failed to open log file\n");                                       \
        }                                                                                       \
    } while (false)

#define ILOG(format, ...) LOG(INF, format, ##__VA_ARGS__)
#define DLOG(format, ...) LOG(DBG, format, ##__VA_ARGS__)
#define ELOG(format, ...) LOG(ERR, format, ##__VA_ARGS__)