// static keyword (globals and locals), goto and labels, multiple
// static locals keeping state between calls.
#include <stdio.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: global int g_counter = 100
// EXPECT-IR: global char g_tag = 65
// EXPECT-IR: global int static_bump_calls = 0
// EXPECT-IR: global int static_accumulate_total_1 = 0
// EXPECT-IR: global int static_nextId_id_2 = 1000

static int g_counter = 100;
static char g_tag = 'A';
static int g_uninit;
static double g_pi = 3.14159;

int bump()
{
    static int calls = 0;        /* persists across calls */
    calls++;
    return calls;
}

int accumulate(int x)
{
    static int total = 0;
    total += x;
    return total;
}

int nextId()
{
    static int id = 1000;
    return ++id;
}

int fibStatic(int n)
{
    static int memo[64];
    if (n < 2) return n;
    if (memo[n] != 0) return memo[n];
    memo[n] = fibStatic(n - 1) + fibStatic(n - 2);
    return memo[n];
}

int gotoMix(int n)
{
    int s = 0;
top:
    if (n <= 0) goto done;
    if (n % 2 == 0) {
        s += n;
        n--;
        goto top;
    }
    s -= 1;
    n--;
    goto top;
done:
    return s;
}

int main()
{
    static int local_static = 5;
    int i;

    printf("%d\n", g_counter);   // 100
    g_counter += 10;
    printf("%d\n", g_counter);   // 110
    printf("%c\n", g_tag);       // A
    printf("%d\n", g_uninit);    // 0 (globals zero-initialised)
    printf("%f\n", g_pi);        // 3.141590

    printf("%d\n", local_static);// 5
    local_static = 50;
    printf("%d\n", local_static);// 50

    printf("%d\n", bump());      // 1
    printf("%d\n", bump());      // 2
    printf("%d\n", bump());      // 3

    printf("%d\n", accumulate(10));  // 10
    printf("%d\n", accumulate(15));  // 25
    printf("%d\n", accumulate(-5));  // 20

    printf("%d\n", nextId());    // 1001
    printf("%d\n", nextId());    // 1002

    printf("%d\n", fibStatic(20));   // 6765

    printf("%d\n", gotoMix(6));  // 6+4+2-1-1-1 = 9
    printf("%d\n", gotoMix(5));  // 4+2-1-1-1 = 3

    for (i = 0; i < 3; i++) {
        static int inside = 77;  /* static in a block */
        inside++;
        printf("%d\n", inside);  // 78 79 80
    }
    return 0;
}
