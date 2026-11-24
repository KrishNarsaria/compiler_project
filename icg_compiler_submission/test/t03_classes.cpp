// Classes, objects, access specifiers, constructors, destructors, inheritance
#include <iostream>
using namespace std;

// EXPECT-IR: class Counter : size 8 { int count @0; int step @4; }
// EXPECT-IR: class Animal : size 20 { int legs @0; char name[16] @4; }
// EXPECT-IR: class Dog : size 24 extends Animal { int legs @0; char name[16] @4; int tricks @20; }
// EXPECT-IR: struct Box : size 8 { int w @0; int h @4; }
// EXPECT-IR: class Res : size 4 { int id @0; }
// EXPECT-IR: func Counter::Counter_void(class Counter* this) : void
// EXPECT-IR: func Counter::Counter_int_int(class Counter* this, int c, int s) : void
// EXPECT-IR: func Counter::tick(class Counter* this) : void
// EXPECT-IR: func Counter::get(class Counter* this) : int
// EXPECT-IR: func Counter::reset(class Counter* this) : void
// EXPECT-IR: func Animal::Animal(class Animal* this, int l) : void
// EXPECT-IR: func Animal::getLegs(class Animal* this) : int
// EXPECT-IR: func Dog::Dog(class Dog* this, int t) : void
// EXPECT-IR: call Animal::Animal, 2
// EXPECT-IR: func Dog::total(class Dog* this) : int
// EXPECT-IR: func Box::area(struct Box* this) : int
// EXPECT-IR: func Res::Res(class Res* this, int i) : void
// EXPECT-IR: func Res::~Res(class Res* this) : void
// EXPECT-IR: call Counter::Counter_int_int, 3
// EXPECT-IR: t23 = alloc 8
// EXPECT-IR: free p
// EXPECT-IR: call Res::~Res, 1
// EXPECT-IR: call __print_int, 1
// EXPECT-IR: call __print_endl, 0

class Counter {
private:
    int count;
    int step;
public:
    Counter() : count(0), step(1) { }
    Counter(int c, int s) : count(c), step(s) { }
    void tick() { count = count + step; }
    int get() const { return count; }
    void reset();
};

void Counter::reset()
{
    count = 0;
}

class Animal {
protected:
    int legs;
    char name[16];
public:
    Animal(int l) { legs = l; }
    int getLegs() { return legs; }
    int speak() { return 0; }
};

class Dog : public Animal {
private:
    int tricks;
public:
    Dog(int t) : Animal(4), tricks(t) { }
    int speak() { return 1; }
    int total() { return legs + tricks; }
};

struct Box {
    int w, h;
    int area() { return w * h; }
};

class Res {
public:
    int id;
    Res(int i) { id = i; printf("acquire %d\n", i); }
    ~Res() { printf("release %d\n", id); }
};

int main()
{
    Counter c;
    Counter d(10, 5);
    c.tick();
    c.tick();
    d.tick();
    printf("%d %d\n", c.get(), d.get());
    c.reset();

    Dog dog(3);
    Animal *a = &dog;
    printf("%d %d %d\n", dog.speak(), dog.total(), a->getLegs());

    Box b;
    b.w = 3;
    b.h = 4;
    cout << b.area() << endl;

    Counter *p = new Counter(1, 2);
    p->tick();
    printf("%d\n", p->get());
    delete p;

    {
        Res r(7);
        printf("in scope\n");
    }
    return 0;
}
