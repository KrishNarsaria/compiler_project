// Literals: every integer base, suffixes, float forms, char escapes,
// string escapes, concatenated strings, bool and nullptr.
#include <stdio.h>

// EXPECT-IR: func main() : int

int main()
{
    /* integer literals in every base */
    int dec = 42;                 // EXPECT-IR: dec = 42
    int hex = 0x2A;               // EXPECT-IR: hex = 42
    int hexUpper = 0XFF;          // EXPECT-IR: hexUpper = 255
    int oct = 052;                // EXPECT-IR: oct = 42
    int bin = 0b101010;           // EXPECT-IR: bin = 42
    int binUpper = 0B101;         // EXPECT-IR: binUpper = 5
    int zero = 0;                 // EXPECT-IR: zero = 0

    /* suffixes */
    int sufU = 10u;               // EXPECT-IR: sufU = 10
    int sufL = 10L;               // EXPECT-IR: sufL = 10
    int sufUL = 10ul;             // EXPECT-IR: sufUL = 10
    int sufLL = 10ll;             // EXPECT-IR: sufLL = 10

    /* floats */
    double f1 = 1.5;              // EXPECT-IR: f1 = 1.5
    double f2 = .5;               // EXPECT-IR: f2 = 0.5
    double f3 = 2.;               // EXPECT-IR: f3 = 2.0
    double f4 = 1e3;              // EXPECT-IR: f4 = 1000
    double f5 = 2.5e-2;           // EXPECT-IR: f5 = 0.025
    double f6 = 3E2;              // EXPECT-IR: f6 = 300
    float f7 = 1.25f;             // EXPECT-IR: f7 = 1.25
    float f8 = 0.5F;              // EXPECT-IR: f8 = 0.5

    /* char escapes */
    char c1 = 'A';                // EXPECT-IR: c1 = 'A'
    char c2 = '\n';               // EXPECT-IR: c2 = '\n'
    char c3 = '\t';               // EXPECT-IR: c3 = '\t'
    char c4 = '\\';               // EXPECT-IR: c4 = '\\'
    char c5 = '\'';               // EXPECT-IR: c5 = '\''
    char c6 = '\0';               // EXPECT-IR: c6 = '\0'
    char c7 = '\x41';             // EXPECT-IR: c7 = 'A'
    char c8 = '\101';             // EXPECT-IR: c8 = 'A'
    char c9 = 'a' - 'a';          // EXPECT-IR: c9 = 0
    char c10 = 65;                // EXPECT-IR: c10 = 65

    /* strings with escapes */
    const char *s1 = "tab\there";         // EXPECT-IR: tab
    const char *s2 = "quote\"back\\slash";
    const char *s3 = "adjacent" " strings";    /* concatenation */
    const char *s4 = "one";
    const char *s5 = "";                       /* empty string */

    /* bool and nullptr */
    bool btrue = true;            // EXPECT-IR: btrue = 1
    bool bfalse = false;          // EXPECT-IR: bfalse = 0
    int *n1 = nullptr;            // EXPECT-IR: n1 = 0
    char *n2 = NULL;              // EXPECT-IR: n2 = 0
    int *n3 = 0;                  // EXPECT-IR: n3 = 0

    printf("%d %d %d %d %d %d %d\n", dec, hex, hexUpper, oct, bin, binUpper, zero);
    printf("%d %d %d %d\n", sufU, sufL, sufUL, sufLL);
    printf("%f %f %f %f %f %f %f %f\n", f1, f2, f3, f4, f5, f6, f7, f8);
    printf("%d %d %d %d %d %d %d %d %d %d\n", c1, c2, c3, c4, c5, c6, c7, c8, c9, c10);
    printf("%s|%s|%s\n", s1, s2, s3);
    printf("%s %s\n", s4, s5);
    printf("%d %d\n", btrue, bfalse);
    printf("%d %d %d\n", n1 == 0, n2 == 0, n3 == 0);
    return 0;
}
