#if !defined(LOGGER_CPP)
#define LOGGER_CPP

#include "logger.h"

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
unique_ptr<Logger>& loggerInstance() { return Logger::getInstance(); }

unique_ptr<Logger>& Logger::init(ostream &o) {
    using std::lock_guard;
    if (loggerPtr2 == nullptr) {
        lock_guard<mutex> guard(loggerMutex);
        if (loggerPtr2 == nullptr)
            loggerPtr2.reset(new Logger(o));
    }
    return loggerPtr2;
}
unique_ptr<Logger>& Logger::init(const string &fpath) {
    using std::lock_guard;
    if (loggerPtr2 == nullptr) {
        lock_guard<mutex> guard(loggerMutex);
        if (loggerPtr2 == nullptr)
            loggerPtr2.reset(new Logger(fpath));
    }
    return loggerPtr2;
}
unique_ptr<Logger>& Logger::getInstance() {
    if (loggerPtr2 == nullptr)
        throw std::runtime_error("Logger is not initialized, call Logger::init first");
    return loggerPtr2;
}

unique_ptr<Logger> Logger::loggerPtr2;
mutex Logger::loggerMutex;

#endif // LOGGER_CPP
