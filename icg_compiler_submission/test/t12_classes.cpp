// Classes and objects: methods (overloaded, const, chained), constructors
// with member initialiser lists, destructors, this pointer, object arrays,
// nested objects, methods defined outside the class.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: func Point::Point_void(class Point* this) : void
// EXPECT-IR: func Point::Point_int_int(class Point* this, int a, int b) : void
// EXPECT-IR: func Point::sum_void(class Point* this) : int
// EXPECT-IR: func Point::sum_int(class Point* this, int extra) : int
// EXPECT-IR: func Rect::Rect(class Rect* this, int x1, int y1, int x2, int y2) : void
// EXPECT-IR: func Rect::shift(class Rect* this, int d) : void
// EXPECT-IR: func Tracker::~Tracker(class Tracker* this) : void

class Point {
public:
    int x, y;
    Point() : x(0), y(0) { }
    Point(int a, int b) : x(a), y(b) { }
    int sum() const { return x + y; }
    int sum(int extra) { return x + y + extra; }     /* overloaded method */
    Point moved(int dx) { Point r(x + dx, y + dx); return r; }
};

class Rect {
private:
    Point tl, br;
public:
    Rect(int x1, int y1, int x2, int y2) : tl(x1, y1), br(x2, y2) { }
    int area() { return (br.x - tl.x) * (br.y - tl.y); }
    void shift(int d);
};

void Rect::shift(int d)
{
    tl.x += d; tl.y += d;
    br.x += d; br.y += d;
}

class Counter {
private:
    int value;
public:
    Counter() : value(0) { }
    Counter(int v) : value(v) { }
    Counter &add(int n) { value += n; return *this; } /* chained, returns *this */
    Counter *self() { return this; }
    int get() const { return value; }
    int calls()
    {
        static int made = 0;      /* static local inside a method */
        made++;
        return made;
    }
};

class Holder {
public:
    Point p;                      /* nested object */
    int tag;
    Holder() : p(9, 9), tag(1) { }
    Holder(int x, int y, int t) : p(x, y), tag(t) { }
};

class Tracker {
public:
    int id;
    Tracker(int i) { id = i; }
    ~Tracker() { printf("gone %d\n", id); }
};

int main()
{
    Point a;
    Point b(3, 4);
    Point *p = new Point(10, 20);
    Point arr[3];                 /* default construction */
    Rect r(0, 0, 4, 5);
    Counter c;
    Counter d(7);
    Holder h;
    Holder h2(1, 2, 3);
    int i;

    printf("%d %d\n", a.sum(), b.sum());            // 0 7
    printf("%d %d\n", b.sum(10), p->sum());         // 17 30
    printf("%d %d\n", b.moved(2).x, b.moved(2).y);  // 5 6
    delete p;

    arr[0].x = 5; arr[0].y = 6;
    arr[1].x = 7; arr[1].y = 8;
    for (i = 0; i < 2; i++)
        printf("%d\n", arr[i].sum());               // 11 15

    printf("%d\n", r.area());                       // 20
    r.shift(1);
    printf("%d\n", r.area());                       // 20

    c.add(5).add(3).add(1);
    printf("%d\n", c.get());                        // 9
    printf("%d\n", c.self() == &c ? 1 : 0);         // 1
    d.add(2);
    printf("%d\n", d.get());                        // 9

    printf("%d\n", c.calls());                      // 1
    printf("%d\n", c.calls());                      // 2

    printf("%d %d %d\n", h.p.x, h.p.y, h.tag);      // 9 9 1
    printf("%d %d %d\n", h2.p.x, h2.p.y, h2.tag);   // 1 2 3

    {
        Tracker t(100);
        printf("inside %d\n", t.id);                // inside 100
    }                                               // dtor: gone 100
    return 0;
}
