/* Ada 83 Runtime Support Functions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Standard file stream accessors for TEXT_IO */
void* __ada_stdin(void) {
    return (void*)stdin;
}

void* __ada_stdout(void) {
    return (void*)stdout;
}

void* __ada_stderr(void) {
    return (void*)stderr;
}

/* Calendar package support */
long long __ada_clock(void) {
    /* Returns current time as seconds since epoch */
    return (long long)time(NULL);
}

int __ada_year(long long t) {
    time_t tt = (time_t)t;
    struct tm *tm = localtime(&tt);
    return tm ? tm->tm_year + 1900 : 1901;
}

int __ada_month(long long t) {
    time_t tt = (time_t)t;
    struct tm *tm = localtime(&tt);
    return tm ? tm->tm_mon + 1 : 1;
}

int __ada_day(long long t) {
    time_t tt = (time_t)t;
    struct tm *tm = localtime(&tt);
    return tm ? tm->tm_mday : 1;
}

double __ada_seconds(long long t) {
    time_t tt = (time_t)t;
    struct tm *tm = localtime(&tt);
    if (!tm) return 0.0;
    return (double)(tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}

long long __ada_time_of(int year, int month, int day, double seconds) {
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    int secs = (int)seconds;
    tm.tm_hour = secs / 3600;
    tm.tm_min = (secs % 3600) / 60;
    tm.tm_sec = secs % 60;
    return (long long)mktime(&tm);
}
