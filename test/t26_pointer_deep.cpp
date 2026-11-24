// Deep pointer semantics: scaled arithmetic for every element size, pointer
// comparison and subtraction, negative offsets, void* conversions, arrays of
// pointers, pointers to pointers, and pointer iteration.
#include <stdio.h>
#include <stdlib.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: local int* p
// EXPECT-IR: local double* dp
// EXPECT-IR: local char* cp
// EXPECT-IR: local struct Box* bp
// EXPECT-IR: p = &arr[8]
// EXPECT-IR: q = p + 12
// EXPECT-IR: back = q - 4
// EXPECT-IR: t4 = q - p
// EXPECT-IR: / 4
// EXPECT-IR: cp2 = cp + 3
// EXPECT-IR: dp2 = dp - 16
// EXPECT-IR: bp2 = bp + 8
// EXPECT-IR: local void* vp
// EXPECT-IR: vp = (void*) p
// EXPECT-IR: local int* fromvoid
// EXPECT-IR: fromvoid = (int*) vp
// EXPECT-IR: local int* pa[3]
// EXPECT-IR: pa[8] = t17
// EXPECT-IR: local int** cur
// EXPECT-IR: cur = &pa
// EXPECT-IR: t21 = *cur
// EXPECT-IR: if p >= q goto
// EXPECT-IR: if p == q goto
// EXPECT-IR: if p != q goto
// EXPECT-IR: *q = 77

struct Box { int w, h; };

int main()
{
    int arr[5] = {10, 20, 30, 40, 50};
    int *p = &arr[2];
    int *q = p + 3;                    /* scale 4: +12 bytes */
    int *back = q - 1;                 /* -4 bytes */
    int n = q - p;                     /* scaled difference, /4 */
    double dv[4] = {1.0, 2.0, 3.0, 4.0};
    double *dp = &dv[3];
    double *dp2 = dp - 2;              /* scale 8: -16 bytes */
    dp2 = dp2 + 1;
    char msg[8] = "abcdefg";
    char *cp = &msg[1];
    char *cp2 = cp + 3;                /* scale 1 */
    struct Box boxes[2];
    struct Box *bp = &boxes[0];
    struct Box *bp2 = bp + 1;          /* scale 8 */
    void *vp = p;                      /* int* -> void* */
    int *fromvoid = (int *)vp;         /* void* -> int* cast */
    int *pa[3];
    pa[0] = &arr[0];
    pa[1] = &arr[4];
    pa[2] = &arr[2];
    int **cur = pa;                    /* int** from array of int* */
    int **last = cur + 2;
    int total = 0;
    while (cur <= last) {
        total += **cur;                /* double deref */
        cur = cur + 1;
    }
    if (p < q)                          /* pointer comparison */
        total += 1;
    if (p != q)
        total += 2;
    if (p == q)
        total = 0;
    *q = 77;
    n = q - p;
    printf("%d %d %f %c %d %d %d\n",
           n, total, *dp2, *cp2, bp2 - bp, *fromvoid, *(int *)vp);
    return 0;
}
