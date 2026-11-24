// Basic features: operators, if-else, loops, switch, arrays, pointers, struct,
// printf/scanf, functions, goto/break/continue, static
#include <stdio.h>

// EXPECT-IR: struct Student : size 12 { int id @0; float marks @4; char grade @8; }
// EXPECT-IR: global int static_counter_calls = 0
// EXPECT-IR: func counter() : int
// EXPECT-IR: static_counter_calls = static_counter_calls + 1
// EXPECT-IR: func maxOf(int a, int b) : int
// EXPECT-IR: if ch == 65 goto
// EXPECT-IR: if ch == 66 goto
// EXPECT-IR: if sum > 25 goto L13
// EXPECT-IR: goto L_done
// EXPECT-IR: L_done:
// EXPECT-IR: call scanf, 2
// EXPECT-IR: call maxOf, 2
// EXPECT-IR: call printf, 5
// EXPECT-IR: p = &a
// EXPECT-IR: s.id = 7
// EXPECT-IR: s.marks = 88.5
// EXPECT-IR: s.grade = 'B'
// EXPECT-IR: word[5] = '\0'

#define MAX 5
#define SQUARE_LIMIT 100

struct Student {
    int id;
    float marks;
    char grade;
};

int counter()
{
    static int calls = 0;
    calls++;
    return calls;
}

int maxOf(int a, int b)
{
    if (a > b)
        return a;
    else
        return b;
}

int main()
{
    int a = 10, b = 3, i, sum = 0;
    float f = 2.5;
    char ch = 'A';
    int arr[MAX] = {5, 3, 8, 1, 9};
    char word[6] = "hello";
    struct Student s;
    int *p = &a;

    /* arithmetic, relational, logical, bitwise */
    int r = (a + b) * 2 - a / b + a % b;
    int bits = (a & b) | (a ^ b) << 1;
    int lg = (a > b && b > 0) || !(a == b);
    f = f * 2 + 1;
    r += 4; r -= 1; r *= 2;
    printf("%d %d %d %f\n", r, bits, lg, f);

    /* loops */
    for (i = 0; i < MAX; i++)
        sum += arr[i];
    i = 0;
    while (i < 3) {
        i++;
        if (i == 2) continue;
        sum += i;
    }
    do {
        sum--;
    } while (sum > 25);

    /* switch */
    switch (ch) {
    case 'A':
        sum += 1;
        break;
    case 'B':
        sum += 2;
        break;
    default:
        sum = 0;
    }

    /* pointers and structure */
    *p = *p + 1;
    s.id = 7;
    s.marks = 88.5;
    s.grade = 'B';
    printf("%d %f %c %s\n", s.id, s.marks, s.grade, word);

    /* goto and break */
    for (i = 0; i < 100; i++) {
        if (i > 3) break;
    }
    if (sum > 1000) goto done;
    sum += counter() + counter();
    scanf("%d", &a);
    printf("max = %d\n", maxOf(a, sum));
done:
    return 0;
}
