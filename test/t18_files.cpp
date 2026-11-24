// File manipulation: text and binary I/O, seeking, appending, renaming,
// error paths - the complete C stdio surface.
#include <stdio.h>
#include <string.h>

// EXPECT-IR: func main() : int
// EXPECT-IR: call fopen, 2
// EXPECT-IR: call fprintf, 3
// EXPECT-IR: call fprintf, 4
// EXPECT-IR: call fwrite, 4
// EXPECT-IR: call fread, 4
// EXPECT-IR: call fseek, 3
// EXPECT-IR: call ftell, 1
// EXPECT-IR: call rename, 2
// EXPECT-IR: call remove, 1

int writeText(const char *path)
{
    FILE *f = fopen(path, "w");
    int i;
    if (f == NULL) return -1;
    fprintf(f, "line %d\n", 1);
    fputs("plain text\n", f);
    fputc('X', f);
    fputc('\n', f);
    for (i = 0; i < 3; i++)
        fprintf(f, "%d %f\n", i, i * 0.5);
    fclose(f);
    return 0;
}

int readText(const char *path)
{
    FILE *f = fopen(path, "r");
    char buf[128];
    int n = 0, sum = 0, v;
    double d;
    if (f == NULL) return -1;
    while (fgets(buf, sizeof(buf), f) != NULL)
        n++;
    rewind(f);
    while (fscanf(f, "%d %lf", &v, &d) == 2)
        sum += v;
    while (!feof(f)) {
        if (fgetc(f) == EOF) break;
        sum++;
    }
    fclose(f);
    return sum + n * 100;
}

int binaryIO(const char *path)
{
    FILE *f = fopen(path, "wb");
    int data[4] = {10, 20, 30, 40};
    int back[4] = {0, 0, 0, 0};
    long pos;
    int i, ok = 1;
    if (f == NULL) return -1;
    fwrite(data, sizeof(int), 4, f);
    fclose(f);

    f = fopen(path, "rb");
    if (f == NULL) return -1;
    pos = ftell(f);
    fseek(f, 2 * sizeof(int), 0);      /* SEEK_SET == 0 */
    fread(back, sizeof(int), 2, f);
    fclose(f);
    if (back[0] != 30 || back[1] != 40) ok = 0;
    return ok;
}

int main()
{
    FILE *f;
    char buf[64];
    int n;

    if (writeText("t18.txt") != 0) { printf("open failed\n"); return 1; }
    printf("%d\n", readText("t18.txt"));      /* 4 lines + (0+1+2) + extra chars */

    f = fopen("t18.txt", "a");                /* append */
    if (f == NULL) { printf("append failed\n"); return 1; }
    fputs("appended\n", f);
    fclose(f);

    f = fopen("t18.txt", "r");
    if (f == NULL) { printf("open failed\n"); return 1; }
    n = 0;
    while (fgets(buf, sizeof(buf), f) != NULL) n++;
    fclose(f);
    printf("%d\n", n);                        /* 5 */

    f = fopen("t18.txt", "r");
    fseek(f, 0, 2);                           /* SEEK_END */
    printf("%ld\n", ftell(f));                /* total size */
    fclose(f);

    printf("%d\n", binaryIO("t18.bin"));      /* 1 */
    printf("%d\n", rename("t18.txt", "t18b.txt"));   /* 0 */
    printf("%d\n", remove("t18b.txt"));       /* 0 */
    printf("%d\n", remove("t18.bin"));        /* 0 */
    printf("%d\n", readText("missing.txt"));  /* -1: fopen fails */
    return 0;
}
