// Function pointers: plain, typedef'd, arrays of them, struct members,
// parameters, return values, and calls through every form.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: local int(int,int)* fp
// EXPECT-IR: local int(int,int)* ops[3]
// EXPECT-IR: func apply(int(int,int)* f, int x, int y) : int
// EXPECT-IR: func pick(int which) : int(int,int)*
// EXPECT-IR: call *

int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }

typedef int (*BinOp)(int, int);
typedef int (*UnOp)(int);

int twice(int x) { return 2 * x; }
int square(int x) { return x * x; }

struct Ops {
    BinOp first;
    BinOp second;
    int flag;
};

int apply(BinOp f, int x, int y) { return f(x, y); }

BinOp pick(int which)
{
    if (which == 1) return add;
    if (which == 2) return sub;
    return mul;
}

UnOp getOp(int which)
{
    return which ? twice : square;
}

int main()
{
    int (*fp)(int, int) = add;        /* plain declaration */
    BinOp ops[3];                     /* array of function pointers */
    struct Ops bag;
    UnOp u;
    BinOp chosen;
    int i, total = 0;

    ops[0] = add;
    ops[1] = sub;
    ops[2] = mul;

    bag.first = add;
    bag.second = mul;
    bag.flag = 1;

    printf("%d\n", fp(2, 3));                  // 5
    fp = &sub;                                 /* &function */
    printf("%d\n", (*fp)(10, 4));              // 6
    printf("%d\n", fp(10, 4));                 // 6

    for (i = 0; i < 3; i++)
        total += ops[i](10, i);                // 10 + 9 + 20 = 39
    printf("%d\n", total);                     // 39

    printf("%d %d\n", bag.first(1, 2), bag.second(3, 4));  // 3 12
    printf("%d\n", apply(mul, 6, 7));          // 42

    chosen = pick(1);
    printf("%d\n", chosen(5, 5));              // 10
    chosen = pick(2);
    printf("%d\n", chosen(5, 5));              // 0
    chosen = pick(0);
    printf("%d\n", chosen(5, 5));              // 25

    u = getOp(0);
    printf("%d\n", u(9));                      // 81
    u = getOp(1);
    printf("%d\n", u(9));                      // 18

    printf("%d\n", (pick(0))(3, 4));           // 12  (call the returned pointer directly)
    return 0;
}
