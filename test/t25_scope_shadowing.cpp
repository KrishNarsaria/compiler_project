// Scoping and shadowing: globals vs locals vs parameters vs block locals,
// same names in different functions, and names that look like temporaries.
#include <stdio.h>

// EXPECT-IR: func main() : int

int value = 1;                    /* global */

int useGlobal() { return value; }

int shadowParam(int value)
{
    return value + 10;            /* parameter shadows global */
}

int shadowLocal()
{
    int value = 100;              /* local shadows global */
    return value;
}

int shadowBlock()
{
    int x = 1;
    {
        int x = 2;                /* inner block shadows outer */
        x = x + 1;
        {
            int x = 40;           /* deeper shadowing */
            x = x + 2;
        }
        return x;                 /* 3: the middle one */
    }
}

int nestedFunc(int n)
{
    int t1 = n;                   /* locals named like temporaries */
    int t2 = t1 + 1;
    int t3 = t2 + 1;
    return t3;
}

int otherFunc(int n)              /* same parameter name, other function */
{
    int n2 = n * 2;
    return n2;
}

int forScope(int n)
{
    int i = 99;
    int sum = 0;
    for (i = 0; i < n; i++) {     /* reuses outer i */
        sum += i;
    }
    return sum * 100 + i;         /* i is n after the loop */
}

int main()
{
    int value = 50;               /* local shadows global */
    int t1 = 7, t2 = 8;           /* temp-like names */

    printf("%d\n", value);        // 50
    printf("%d\n", useGlobal());  // 1
    printf("%d\n", shadowParam(5));   // 15
    printf("%d\n", shadowLocal());    // 100
    printf("%d\n", shadowBlock());    // 3
    printf("%d\n", nestedFunc(1));    // 4
    printf("%d\n", otherFunc(21));    // 42
    printf("%d\n", forScope(4));      // (0+1+2+3)*100 + 4 = 604
    printf("%d %d\n", t1, t2);    // 7 8

    {
        int t1 = 11;              /* shadow a temp-like name in a block */
        printf("%d\n", t1);       // 11
    }
    printf("%d\n", t1);           // 7
    return 0;
}
