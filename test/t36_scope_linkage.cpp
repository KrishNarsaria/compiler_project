// Scope and linkage: globals vs statics vs locals, shadowing globals and
// parameters, block-level redeclaration, global arrays/structs with
// initialisers, global constants, forward references to functions defined
// later, and shadowing across nested blocks.
#include <stdio.h>

// EXPECT-IR: global int g_total = 10    ; static (internal linkage)
// EXPECT-IR: global int g_shared = 0
// EXPECT-IR: global char g_name[8] = "gvar"
// EXPECT-IR: global struct Pt g_pt = {3, 4}
// EXPECT-IR: global int ghidden = 5    ; static (internal linkage)
// EXPECT-IR: func late(int x) : int
// EXPECT-IR: func reader() : int
// EXPECT-IR: return g_total
// EXPECT-IR: func writer(int v) : int
// EXPECT-IR: g_total = v
// EXPECT-IR: func shadow(int x) : int
// EXPECT-IR: local int x_1
// EXPECT-IR: t1 = x_1 + 7
// EXPECT-IR: func main() : int
// EXPECT-IR: local int g_total_1
// EXPECT-IR: g_total_1 = 100
// EXPECT-IR: local int g_shared_1
// EXPECT-IR: g_shared_1 = 200
// EXPECT-IR: local int inner
// EXPECT-IR: inner = 1
// EXPECT-IR: local int inner_1
// EXPECT-IR: inner_1 = 2
// EXPECT-IR: call late, 1
// EXPECT-IR: call reader, 0
// EXPECT-IR: call writer, 1
// EXPECT-IR: call shadow, 1

static int g_total = 10;
int g_shared = 0;
char g_name[8] = "gvar";
struct Pt { int x, y; };
struct Pt g_pt = { 3, 4 };
static int ghidden = 5;

int late(int x);                       /* forward declaration */

int reader()
{
    return g_total + g_shared + ghidden;
}

int writer(int v)
{
    g_total = v;
    g_shared = g_shared + v;
    return g_total;
}

int shadow(int x)
{
    {
        int x = 100;                   /* parameter shadowed in an inner block */
        return x + 7;
    }
}

int main()
{
    int g_total = 100;                 /* global shadowed by local */
    int g_shared = 200;
    int inner = 1;
    {
        int inner = 2;                 /* block redeclaration */
        g_shared = g_shared + inner;
    }
    g_shared = g_shared + inner;
    g_total = g_total + reader();
    g_total = writer(g_total);
    printf("%d %d %d %d %d %c\n",
           g_total, g_shared, late(4), shadow(3), g_pt.x + g_pt.y, g_name[0]);
    return 0;
}

int late(int x)
{
    return x * 3;
}
