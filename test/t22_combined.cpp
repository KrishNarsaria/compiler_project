// Integration test: most features working together in one program -
// classes with inheritance, dynamic allocation, lambdas, function
// pointers, variadics, files, statics, references, until loops.
#include <stdio.h>
#include <stdarg.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: class Circle : size 8 extends Shape
// EXPECT-IR: func Circle::Circle(class Circle* this, int i, int r) : void
// EXPECT-IR: func Circle::area(class Circle* this) : int
// EXPECT-IR: func vsum(int n, ...) : int
// EXPECT-IR: va_start n
// EXPECT-IR: local int(int,int)* table[2]
// EXPECT-IR: call *

class Shape {
protected:
    int id;
public:
    Shape(int i) : id(i) { }
    int getId() { return id; }
    int kind() { return 0; }
    ~Shape() { printf("~shape %d\n", id); }
};

class Circle : public Shape {
private:
    int radius;
public:
    Circle(int i, int r) : Shape(i), radius(r) { }
    int area() { return 3 * radius * radius; }
    int kind() { return 1; }
};

typedef int (*Op)(int, int);

int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }

int vsum(int n, ...)
{
    va_list ap;
    int s = 0, i;
    va_start(ap, n);
    for (i = 0; i < n; i++) s += va_arg(ap, int);
    va_end(ap);
    return s;
}

int useCounter()
{
    static int calls = 0;
    return ++calls;
}

int main()
{
    static int gtotal = 0;
    Circle c1(1, 5);
    Circle *c2 = new Circle(2, 3);
    Op table[2];
    int data[4] = {3, 1, 4, 1};
    int i, sum = 0;
    FILE *f;

    table[0] = add;
    table[1] = mul;

    auto lam = [&](int v) { gtotal += v; return gtotal; };
    auto lam2 = [=](int v) { return v * 2; };

    for (i = 0; i < 4; i++)
        sum += table[i % 2](data[i], 2);   /* 5 + 2 + 6 + 3 = 16 */

    until (sum >= 100) {
        sum += lam2(sum);                  /* doubles until >= 100 */
    }

    printf("%d\n", c1.area());             // 75
    printf("%d\n", c2->area());            // 27
    printf("%d\n", c1.getId() + c2->getId());   // 3
    printf("%d\n", c1.kind());             // 1
    printf("%d\n", sum);                   // 128
    printf("%d\n", vsum(4, 1, 2, 3, 4));   // 10
    printf("%d\n", lam(10));               // gtotal = 10
    printf("%d\n", lam(5));                // 15
    int uc1 = useCounter();
    int uc2 = useCounter();
    printf("%d %d\n", uc1, uc2);   // 1 2

    f = fopen("t22.txt", "w");
    if (f != NULL) {
        fprintf(f, "integrated %d\n", sum);
        fclose(f);
    }
    f = fopen("t22.txt", "r");
    if (f != NULL) {
        int got = 0;
        if (fscanf(f, "integrated %d", &got) == 1)
            printf("%d\n", got);           // 128
        fclose(f);
        remove("t22.txt");
    }

    delete c2;
    return 0;
}
