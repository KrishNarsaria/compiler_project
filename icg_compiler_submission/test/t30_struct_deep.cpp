// Struct/union depth: nested structs, arrays of structs, whole-struct copy,
// struct return and pass-by-value, struct inside union, structs with arrays
// and pointers, deep member chains, struct methods, enum fields.
#include <stdio.h>
#include <string.h>

// EXPECT-IR: struct Inner : size 8 { int a @0; int b @4; }
// EXPECT-IR: struct Outer : size 24 { struct Inner in @0; int arr[2] @8; int* ref @16; }
// EXPECT-IR: union Mix : size 16 { struct Inner in @0; double d @0; int raw[4] @0; }
// EXPECT-IR: struct Item : size 8 { char tag @0; int val @4; }
// EXPECT-IR: func makeInner(int x, int y) : struct Inner
// EXPECT-IR: func shiftInner(struct Inner q, int dx) : struct Inner
// EXPECT-IR: func sumItems(struct Item* it, int n) : int
// EXPECT-IR: local struct Outer o
// EXPECT-IR: o.in = t1
// EXPECT-IR: t2[0] = 7
// EXPECT-IR: t3[4] = 8
// EXPECT-IR: o.ref = t5
// EXPECT-IR: local struct Item items[3]
// EXPECT-IR: items[8] = t7
// EXPECT-IR: items[16] = t8
// EXPECT-IR: local struct Outer big[2]
// EXPECT-IR: big[0] = o
// EXPECT-IR: t10->in = t12
// EXPECT-IR: local union Mix m
// EXPECT-IR: m.in.a = 11
// EXPECT-IR: t13[4] = 99
// EXPECT-IR: local struct Item back
// EXPECT-IR: back = call makeItem, 1
// EXPECT-IR: sum = t19 + t20
// EXPECT-IR: t17 = t16[4]
// EXPECT-IR: local int* q2
// EXPECT-IR: q2 = &o.in.a
// EXPECT-IR: t36 = *q2
// EXPECT-IR: t39 = call sumItems, 2
// EXPECT-IR: func makeItem(int v) : struct Item

struct Inner {
    int a;
    int b;
};

struct Outer {
    struct Inner in;
    int arr[2];
    int *ref;
};

union Mix {
    struct Inner in;
    double d;
    int raw[4];
};

struct Item {
    char tag;
    int val;
};

struct Inner makeInner(int x, int y)
{
    struct Inner r;
    r.a = x;
    r.b = y;
    return r;
}

struct Inner shiftInner(struct Inner q, int dx)
{
    q.a = q.a + dx;
    q.b = q.b + dx;
    return q;
}

struct Item makeItem(int v)
{
    struct Item it;
    it.tag = 'I';
    it.val = v;
    return it;
}

int sumItems(struct Item *it, int n)
{
    int i, s = 0;
    for (i = 0; i < n; i++)
        s = s + it[i].val;
    return s;
}

int main()
{
    struct Outer o;
    o.in = makeInner(3, 4);
    o.arr[0] = 7;
    o.arr[1] = 8;
    o.ref = &o.arr[0];

    struct Item items[3];
    items[0] = makeItem(1);
    items[1] = makeItem(2);
    items[2] = makeItem(3);
    items[1].tag = 'x';

    struct Outer big[2];
    big[0] = o;
    big[1].in = shiftInner(o.in, 10);

    union Mix m;
    m.in.a = 11;
    m.in.b = 12;
    m.raw[1] = 99;                       /* union reinterpretation */

    struct Item back = makeItem(3);      /* return-by-value init */
    struct Inner deep = makeInner(o.in.a, o.ref[1]);

    int sum = o.in.a + o.in.b;
    sum = sum + o.ref[1] + big[1].in.b + back.val + deep.b + items[2].val;
    int *q2 = &o.in.a;                   /* pointer into nested member */
    sum = sum + *q2;

    sum = sum + sumItems(items, 3);
    printf("%d %d %d %c %d\n", sum, big[0].arr[0], m.raw[1], items[1].tag, m.in.b);
    return 0;
}
