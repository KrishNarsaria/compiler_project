// Inheritance: single inheritance, multi-level, overriding, protected
// access from derived classes, base constructors with initialiser lists,
// upcasts, field hiding, destructors on both levels.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: class Square : size 24 extends Shape
// EXPECT-IR: class Cube : size 28 extends Square
// EXPECT-IR: class Car : size 8 extends Vehicle
// EXPECT-IR: func Square::Square(class Square* this, int l) : void
// EXPECT-IR: func Cube::Cube(class Cube* this, int l, int d) : void
// EXPECT-IR: func Cube::volume(class Cube* this) : int
// EXPECT-IR: func Car::describe(class Car* this) : int

class Shape {
protected:
    int sides;
    char name[16];
public:
    Shape(int s) : sides(s) { name[0] = 0; }
    int getSides() { return sides; }
    int kind() { return 0; }
    ~Shape() { printf("~Shape %d\n", sides); }
};

class Square : public Shape {
protected:
    int side_len;                 /* protected: visible to derived classes */
public:
    Square(int l) : Shape(4), side_len(l) { }
    int getLen() { return side_len; }
    int area() { return side_len * side_len; }      /* overrides */
};

class Cube : public Square {
private:
    int depth;
public:
    Cube(int l, int d) : Square(l), depth(d) { }
    int volume() { return depth * side_len * getLen(); } /* uses inherited member */
    int area() { return 6 * getLen() * getLen(); }
};

class Vehicle {
protected:
    int wheels;
public:
    Vehicle(int w) : wheels(w) { }
    int getWheels() { return wheels; }
    int describe() { return wheels * 10; }
};

class Car : public Vehicle {
private:
    int wheels;                   /* hides the base field */
public:
    Car() : Vehicle(0), wheels(4) { }
    int describe() { return wheels * 100; }         /* hides base method */
    int baseDescribe() { return getWheels() * 10; }
};

class Point {
public:
    int x, y;
    Point(int a, int b) : x(a), y(b) { }
};

class NamedPoint : public Point {
public:
    int z;
    NamedPoint(int a, int b, int c) : Point(a, b), z(c) { }
    int sum() { return x + y + z; }                 /* protected-like access via public base */
};

int main()
{
    Square s(5);
    Cube c(2, 3);
    Shape *shp = &s;              /* upcast */
    Car car;
    NamedPoint np(1, 2, 3);
    Square *sq = new Square(7);
    Cube *cu = new Cube(4, 5);

    printf("%d\n", s.getSides());       // 4
    printf("%d\n", s.kind());           // 0 (inherited)
    printf("%d\n", s.area());           // 25 (override)
    printf("%d\n", c.getSides());       // 4 (multi-level)
    printf("%d\n", c.volume());         // 3 * 2 * 2 = 12
    printf("%d\n", c.area());           // 24 (most-derived override)
    printf("%d\n", shp->getSides());    // 4 (through base pointer)
    printf("%d\n", car.describe());     // 400
    printf("%d\n", car.baseDescribe()); // 0
    printf("%d\n", car.getWheels());    // 0
    printf("%d\n", np.sum());           // 6
    printf("%d\n", sq->area());         // 49
    printf("%d\n", cu->volume());       // 5 * 4 * 4 = 80
    delete sq;
    delete cu;
    return 0;
}
