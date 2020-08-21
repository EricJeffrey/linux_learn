
#include "unistd.h"
#include "Logger.h"
#include "ThreadPool.h"

class SampleTask : public Task {
private:
    int x;

public:
    SampleTask(int x) : x(x) {}
    ~SampleTask() {}
    void run() override {
        loggerInstance()->info({"SampleTask: ", to_string(x), " running..."});
        usleep(100000);
        loggerInstance()->info({"SampleTask: ", to_string(x), " exiting..."});
    }
};

void testThreadPool() {
    loggerInstance()->debug("executing testThreadPool");
    ThreadPool *poolPtr = new ThreadPool();
    for (size_t i = 0; i < 2000000; i++) {
        poolPtr->addTask(make_shared<SampleTask>(i));
        // usleep(1000000);
    }
}

int main(int argc, char const *argv[]) {
    try {
        Logger::init("./foo.txt");
        loggerInstance()->setDebug(false);
        testThreadPool();
        loggerInstance()->info("main sleep now");
        while (true)
            ;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }

    return 0;
}