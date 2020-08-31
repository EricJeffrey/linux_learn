#if !defined(LOGGER_H)
#define LOGGER_H

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

using std::endl;
using std::initializer_list;
using std::lock_guard;
using std::mutex;
using std::ofstream;
using std::ostream;
using std::string;
using std::thread;
using std::to_string;
using std::unique_ptr;

class Logger;
string curTime();
Logger *loggerInstance();

class Logger {
private:
    static Logger *loggerPtr;
    static mutex loggerMutex;

    bool debugOn = false;
    unique_ptr<ostream> out;
    // log into ostream, default [stderr]
    Logger(ostream &o = std::cerr) : out(&o) {}
    // log into [filePath] with append
    Logger(const string &filePath) : out(new ofstream(filePath, std::ios::out)) {}
    ~Logger() {}
    // sync output
    void logOut(initializer_list<string> sli, const string &tag) {
        lock_guard<mutex> lock(loggerMutex);
        (*out) << curTime() << " " << tag << " ";
        for (auto &&s : sli)
            (*out) << s << " ";
        (*out) << endl;
    }

public:
    void operator=(const Logger &) = delete;
    Logger(const Logger &) = delete;

    static Logger *init(ostream &o);
    static Logger *init(const string &fpath);
    static Logger *getInstance();

    void setDebug(bool dbgOn = false) { debugOn = dbgOn; }
    void debug(initializer_list<string> strList) {
        if (debugOn)
            logOut(strList, "DEBUG");
    }
    void debug(const string &s) { debug({s}); }
    void info(initializer_list<string> strList) { logOut(strList, "INFO"); }
    void info(const string &s) { info({s}); }
    void warn(initializer_list<string> strList) { logOut(strList, "WARN"); }
    void warn(const string &s) { warn({s}); }
    void error(initializer_list<string> strList) { logOut(strList, "ERROR"); }
    void error(const string &s) { error({s}); }
    void sysError(int tmpErrno, const string &s) {
        logOut({s, "errno:", to_string(tmpErrno), strerror(tmpErrno)}, "SYS-ERROR");
    };
};
#endif // LOGGER_H
