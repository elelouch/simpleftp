#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE 128

int main(int argc, char *argv[])
{
    char la_fija[] = "cancer";
    int lafija_size = sizeof(la_fija) / sizeof(char);
    char *input = malloc(lafija_size);
    char *res = NULL;
    char *res2 = NULL;
    strncpy(input, la_fija, lafija_size);
    res2 = strtok_r(input, "n", &res);
    printf("%s", res2);
    printf("%s", res);
    return 0;
}
