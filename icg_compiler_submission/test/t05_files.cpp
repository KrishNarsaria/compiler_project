// File manipulation with the C stdio API
#include <cstdio>
#include <cstring>

// EXPECT-IR: local struct FILE* fp
// EXPECT-IR: fp = call fopen, 2
// EXPECT-IR: t4 = call fprintf, 3
// EXPECT-IR: call fputs, 2
// EXPECT-IR: call fclose, 1
// EXPECT-IR: t10 = call fgets, 3
// EXPECT-IR: call strlen, 1
// EXPECT-IR: call rewind, 1
// EXPECT-IR: t16 = call fscanf, 3
// EXPECT-IR: call feof, 1
// EXPECT-IR: call remove, 1

int main()
{
    FILE *fp = fopen("data.txt", "w");
    char line[64];
    int n, i, value;
    if (fp == NULL) {
        printf("cannot open file\n");
        return 1;
    }
    for (i = 1; i <= 3; i++)
        fprintf(fp, "%d\n", i * 10);
    fputs("end\n", fp);
    fclose(fp);

    fp = fopen("data.txt", "r");
    n = 0;
    while (fgets(line, 64, fp) != NULL) {
        n++;
        if (strlen(line) > 3) break;
    }
    rewind(fp);
    fscanf(fp, "%d", &value);
    while (!feof(fp)) {
        if (fscanf(fp, "%d", &value) != 1) break;
        n += value;
    }
    fclose(fp);
    remove("data.txt");
    printf("%d\n", n);
    return 0;
}
