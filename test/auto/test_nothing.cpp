#include <iostream>
#include <memory>

class Base {
    public:
    int b;
    virtual void foo() {
        std::cout << "Base: " << sizeof(this) << std::endl;
    }
};

class A : public Base {
public:
    int a;
    int c;
    int d;
    void foo() {
        std::cout << "A: " << sizeof(this) << std::endl;
    }
};

int main(int argc, const char** argv) {
    std::unique_ptr<Base> a = std::make_unique<A>();
    A b;
    A c; 
    std::cout << "hello world!" << std::endl;
    a->foo();
    std::cout << "sizeof(Base) = " << sizeof(Base) << std::endl;
    std::cout << "sizeof(A) = " << sizeof(A) << std::endl;
    std::cout << "sizeof(pointer) = " << sizeof(void*) << std::endl;
    c.foo();
    return 0;
}