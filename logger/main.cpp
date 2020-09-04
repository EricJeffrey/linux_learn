// #include "logger.h"

/* 函数模板学习 */

#include "logger.h"

int main(int argc, char const *argv[]) {

    // Logger::init("./foo.log");
    Logger::init(std::cout);
    loggerInstance()->setDebug(true);
    loggerInstance()->info(1 / 3.0, "sdf", 2233);
    loggerInstance()->debug(1 / 3.0, "sdf", 2233);
    loggerInstance()->sysError(EACCES, 1 / 3.0, "sdf", 2233);
    loggerInstance()->error(1 / 3.0, "sdf", 2233);
    loggerInstance()->setDebug(false);
    loggerInstance()->setPrecision(2);
    loggerInstance()->setEnding(" --\n");
    loggerInstance()->setSeparator("\t");
    loggerInstance()->info(1 / 3.0, "sdf", 2233);
    loggerInstance()->debug(1 / 3.0, "sdf", 2233);
    loggerInstance()->sysError(EACCES, 1 / 3.0, "sdf", 2233);
    loggerInstance()->error(1 / 3.0, "sdf", 2233);
    return 0;
}
