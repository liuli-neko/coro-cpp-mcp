#include <iostream>
#include <memory>

class Base {
    public:
    int b;
};

class A : public Base {
public:
    int a;
};

int main(int argc, const char** argv) {
    std::unique_ptr<Base> a = std::make_unique<A>();
    std::cout << "hello world!" << std::endl;
    return 0;
}