#if !defined(LOGGER_H)
#define LOGGER_H

#include <iomanip>
#include <cstring>
#include <thread>
#include <mutex>
#include <iostream>
#include <fstream>
#include <ostream>
#include <string>
#include <memory>
#include <mutex>
#include <ctime>
#include <stdexcept>
#include <exception>
#include <any>

using std::lock_guard;
using std::mutex;
using std::ofstream;
using std::ostream;
using std::string;
using std::unique_ptr;

class Logger;
string curTime();
unique_ptr<Logger> &loggerInstance();

class Logger {
private:
    static unique_ptr<Logger> loggerPtr2;
    static mutex loggerMutex;

    std::ostream *out;

    bool debugOn;
    bool outputTime;
    string separator;
    string ending;
    bool shouldBeDestroyed;

    Logger(ostream *o, bool destroy = false)
        : out(o), debugOn(false), outputTime(true), separator(" "), ending("\n"),
          shouldBeDestroyed(destroy) {}
    // log into ostream, default [stderr]
    Logger(ostream &o) : Logger(&o) {}
    // log into [filePath] with append
    Logger(const string &filePath) : Logger(new ofstream(filePath, std::ios::out), true) {}

    template <typename T> void logout(const T &t) { (*out) << t << ending; }

    template <typename T, typename... Args> void logout(const T &t, const Args &... args) {
        (*out) << t << separator;
        logout(args...);
    }

public:
    void operator=(const Logger &) = delete;
    Logger(const Logger &) = delete;
    ~Logger() {
        if (shouldBeDestroyed)
            delete out;
    }

    static unique_ptr<Logger> &init(ostream &o);
    static unique_ptr<Logger> &init(const string &fpath);
    static unique_ptr<Logger> &getInstance();

    void setDebug(bool on = false) { debugOn = on; }

    void setPrecision(int n) { (*out) << std::fixed << std::setprecision(n); }

    void setSeparator(const string &sep = " ") { this->separator = sep; }

    void setEnding(const string &ending = "\n") { this->ending = ending; }

    template <typename... Args> void debug(const Args &... args) {
        const string tag("DEBUG");
        if (debugOn && outputTime)
            logout(curTime(), tag, args...);
        else if (debugOn && !outputTime)
            logout(tag, args...);
    }

    template <typename... Args> void info(const Args &... args) {
        const string tag("INFO");
        outputTime ? logout(curTime(), tag, args...) : logout(tag, args...);
    }
    template <typename... Args> void warn(const Args &... args) {
        const string tag("WARN");
        outputTime ? logout(curTime(), tag, args...) : logout(tag, args...);
    }
    template <typename... Args> void error(const Args &... args) {
        const string tag("ERROR");
        outputTime ? logout(curTime(), tag, args...) : logout(tag, args...);
    }
    template <typename... Args> void sysError(int tmpErrno, const Args &... args) {
        const string tag("SYS-ERROR");
        outputTime ? logout(curTime(), tag, args..., "errno:", tmpErrno, strerror(tmpErrno))
                   : logout(tag, args..., "errno:", tmpErrno, strerror(tmpErrno));
    };
};

#endif // LOGGER_H
