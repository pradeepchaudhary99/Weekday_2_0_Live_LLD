#include <iostream>

struct Request {
    int priority = 0;
};

class IHandler {
public:
    virtual ~IHandler() = default;

    IHandler* setNextHandler(IHandler* handler) {
        nextHandler_ = handler;
        return nextHandler_;
    }

    IHandler* getNextHandler() const {
        return nextHandler_;
    }

    virtual bool isAllowed(const Request& request) = 0;
    virtual void processRequest(const Request& request) = 0;

private:
    IHandler* nextHandler_ = nullptr;
};

class Level1Handler : public IHandler {
public:
    bool isAllowed(const Request& request) override {
        return request.priority < 8;
    }

    void processRequest(const Request& request) override {
        if (isAllowed(request)) {
            std::cout << "Processing the request : lvel1 is processing" << std::endl;
        } else {
            getNextHandler()->processRequest(request);
        }
    }
};

class Level2Handler : public IHandler {
public:
    bool isAllowed(const Request& request) override {
        return request.priority < 15;
    }

    void processRequest(const Request& request) override {
        if (isAllowed(request)) {
            std::cout << "Processing the request : lvel1 is processing" << std::endl;
        } else {
            getNextHandler()->processRequest(request);
        }
    }
};

class Level3Handler : public IHandler {
public:
    bool isAllowed(const Request& request) override {
        return request.priority < 2;
    }

    void processRequest(const Request& request) override {
        if (isAllowed(request)) {
            std::cout << "Processing the request : lvel1 is processing" << std::endl;
        } else {
            getNextHandler()->processRequest(request);
        }
    }
};

int main() {
    Level1Handler level1;
    Level2Handler level2;
    Level3Handler level3;

    level1.setNextHandler(&level2)->setNextHandler(&level3);

    return 0;
}
