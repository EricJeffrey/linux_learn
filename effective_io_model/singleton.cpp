
#include <mutex>

class Singleton {
private:
    static Singleton *singleton;
    static std::mutex singletonMutex;

    int x;

protected:
    Singleton() {}
    ~Singleton() {}

public:
    void operator=(const Singleton &) = delete;
    Singleton(const Singleton &) = delete;

    int getX() { return x; }
    void setX(int x) { this->x = x; }

    static Singleton *getInstance();
};

Singleton *Singleton::getInstance() {
    if (singleton == nullptr) {
        std::lock_guard<std::mutex> lockgard(singletonMutex);
        if (singleton == nullptr)
            singleton == new Singleton();
    }
    return singleton;
}

int main(int argc, char const *argv[]) {
    Singleton *instance = Singleton::getInstance();
    instance->setX(2333);
    return 0;
}
