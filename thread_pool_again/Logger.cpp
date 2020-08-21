#if !defined(LOGGER_CPP)
#define LOGGER_CPP

#include "Logger.h"

// get current time -> xxxx-xx-xx xx:xx:xx
string curTime() {
    using std::runtime_error;
    time_t t = time(NULL);
    struct tm ttm = *localtime(&t);
    const int BUF_SZ = 30, TARGET_SZ = 19;
    char buf[BUF_SZ] = {"Error On Call to snprintf"};
    int ret = snprintf(buf, BUF_SZ, "%d-%02d-%02d %02d:%02d:%02d", ttm.tm_year + 1900,
                       ttm.tm_mon + 1, ttm.tm_mday, ttm.tm_hour, ttm.tm_min, ttm.tm_sec);
    if (ret > 0)
        memset(buf + TARGET_SZ, 0, BUF_SZ - TARGET_SZ);
    return buf;
}
Logger *loggerInstance() { return Logger::getInstance(); }

Logger *Logger::init(ostream &o) {
    using std::lock_guard;
    if (loggerPtr == nullptr) {
        lock_guard<mutex> guard(loggerMutex);
        if (loggerPtr == nullptr)
            loggerPtr = new Logger(o);
    }
    return loggerPtr;
}
Logger *Logger::init(const string &fpath) {
    using std::lock_guard;
    if (loggerPtr == nullptr) {
        lock_guard<mutex> guard(loggerMutex);
        if (loggerPtr == nullptr)
            loggerPtr = new Logger(fpath);
    }
    return loggerPtr;
}
Logger *Logger::getInstance() { return loggerPtr; }

Logger *Logger::loggerPtr;
mutex Logger::loggerMutex;

#endif // LOGGER_CPP
