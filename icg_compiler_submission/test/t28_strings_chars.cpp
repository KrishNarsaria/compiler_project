// Strings and characters: literal concatenation, escape sequences (hex, octal,
// common C escapes), char arithmetic and comparison, string functions
// (strlen/strcpy/strcat/strcmp/strncmp/strchr/strstr/strncpy), char indexing,
// and char* assignment from literals.
#include <stdio.h>
#include <string.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: msg = "hello world"
// EXPECT-IR: esc[0] = 'a'
// EXPECT-IR: esc[1] = '\n'
// EXPECT-IR: esc[2] = '\t'
// EXPECT-IR: esc[3] = '\0'
// EXPECT-IR: esc[4] = 'B'
// EXPECT-IR: esc[5] = 'B'
// EXPECT-IR: esc[6] = 'A'
// EXPECT-IR: local char buf[64]
// EXPECT-IR: t1 = call strlen, 1
// EXPECT-IR: c1 = call strcpy, 2
// EXPECT-IR: c2 = call strcat, 2
// EXPECT-IR: cmp = call strcmp, 2
// EXPECT-IR: cmp2 = call strncmp, 3
// EXPECT-IR: hit = call strchr, 2
// EXPECT-IR: sub = call strstr, 2
// EXPECT-IR: cpy = call strncpy, 3
// EXPECT-IR: t26 = call putchar, 1
// EXPECT-IR: t27 = call getchar, 0
// EXPECT-IR: buf[i] = t18
// EXPECT-IR: t23 = *p
// EXPECT-IR: p = p + 1
// EXPECT-IR: local char* d
// EXPECT-IR: d = "short"

int main()
{
    char *msg = "hello " "world";              /* literal concatenation */
    char esc[8] = "a\n\t\0B\x42\101";          /* mixed escapes */
    char buf[64];
    char *p;
    char ch;
    int i;

    int len = strlen(msg);
    char *c1 = strcpy(buf, msg);
    char *c2 = strcat(buf, "!!");
    int cmp = strcmp(buf, "hello world!!");
    int cmp2 = strncmp(buf, "hello", 5);
    char *hit = strchr(buf, 'o');
    char *sub = strstr(buf, "world");
    char *cpy = strncpy(buf, msg, 4);
    (void)c1; (void)c2; (void)cmp; (void)cmp2;
    (void)hit; (void)sub; (void)cpy;

    /* char arithmetic and comparison */
    ch = 'a';
    for (i = 0; i < 5; i++) {
        buf[i] = ch + i;                       /* 'a'..'e' */
    }
    if (ch < 'z')
        ch = ch + 1;
    if (ch >= 'a' && ch <= 'z')
        len = len + 1;

    /* iterate a string with a pointer */
    p = msg;
    while (*p != '\0') {
        len = len + 1;
        p = p + 1;
    }

    putchar(ch);
    ch = getchar();
    char *d = "short";
    printf("%d %d %c %c %s\n", len, msg[5], buf[0], esc[4], d);
    return 0;
}
