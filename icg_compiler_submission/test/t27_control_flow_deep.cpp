// Control-flow torture: nested if/else chains, switch fallthrough groups,
// switch with only default, nested loops with break/continue, continue in
// do-while, while(1) with break, until loops, goto across scopes, dangling
// else, ternary chains, comma in loop heads.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: func classify(int n) : int
// EXPECT-IR: if n >= 0 goto
// EXPECT-IR: if n != 0 goto
// EXPECT-IR: if n > 100 goto
// EXPECT-IR: if n > 999 goto
// EXPECT-IR: if n < 1000 goto
// EXPECT-IR: func tally(int v) : int
// EXPECT-IR: if v == 1 goto
// EXPECT-IR: if v == 2 goto
// EXPECT-IR: if v == 3 goto
// EXPECT-IR: if v == 9 goto
// EXPECT-COUNT: ifFalse flag goto 1
// EXPECT-IR: goto L_done
// EXPECT-IR: L_done:

int classify(int n)
{
    if (n < 0)
        return -1;
    else if (n == 0)
        return 0;
    else if (n <= 100)
        return 1;
    else if (n <= 999)
        return 2;
    else
        return 3;
    if (n >= 1000)
        return 4;
    return -2;
}

int tally(int v)
{
    int t = 0;
    switch (v) {
    case 1:
        t = t + 10;              /* fallthrough */
    case 2:
        t = t + 20;              /* fallthrough */
    case 3:
        t = t + 30;
        break;
    case 9:
        t = t * 2;
        break;
    default:
        t = -1;
    }
    return t;
}

int main()
{
    int i, j, acc = 0;
    int hit = 0;

    /* nested loops: continue skips only the innermost */
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            if (j == i) continue;
            acc = acc + i * 10 + j;
        }
    }

    /* labelled break simulation with flags */
    int flag = 0;
    for (i = 0; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            if (i * j > 20) { flag = 1; break; }
            acc = acc + 1;
        }
        if (flag) break;
    }

    /* while(1) with break, and continue */
    i = 0;
    while (1) {
        i = i + 1;
        if (i % 2 == 0) continue;
        if (i > 10) break;
        acc = acc + i;
    }

    /* do-while with continue jumps to the condition */
    i = 0;
    do {
        i = i + 1;
        if (i % 3 == 0) continue;
        if (i > 20) break;
        acc = acc - 1;
    } while (1);

    /* until loop variants */
    j = 0;
    until (j >= 4) {
        j = j + 1;
    }
    acc = acc + j;
    do {
        j = j - 2;
    } until (j <= 0);
    acc = acc + j;

    /* dangling else binds to the nearest if */
    i = 7;
    if (i > 5)
        if (i > 6)
            acc = acc + 1;
        else
            acc = acc + 100;
    else
        acc = acc + 1000;

    /* goto out of a loop */
    for (i = 0; i < 5; i++) {
        if (i == 2) goto done;
        acc = acc + i;
    }
done:

    /* switch with only default */
    switch (acc % 7) {
    default:
        acc = acc + 5;
    }

    /* ternary chain and comma in for head */
    int k = (acc > 100 ? (acc > 200 ? 3 : 2) : 1);
    int f, g;
    for (f = 0, g = 10; f < 3; f++, g--) {
        acc = acc + (g > 0 ? g : -g);
    }
    acc = acc + k;

    printf("%d %d %d %d\n", classify(-5), classify(0), classify(50), classify(5000));
    printf("%d %d\n", tally(1), tally(9));
    printf("%d %d %d %d %d %d\n", acc, hit, i, j, f, g);
    return 0;
}
