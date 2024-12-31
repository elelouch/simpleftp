#include <err.h>
#include <string.h>
#include <stdio.h>

struct pedro {
    int hola;
    int chau;
};

int main(int c, char *argv[])
{
    char buff[] = "Entering Passive Mode (123,456,789,101,4,12)";
    char venom[256];
    char *test;
    int upper_port = 0;
    int lower_port = 0;

    printf("%s\n", strtok(buff, "()"));
    test = strtok(NULL, "()");
    printf("%s\n", test);

    sscanf(test, "%*d,%*d,%*d,%*d,%d,%d", &upper_port, &lower_port);
    sprintf(venom, "%d", upper_port * 256 + lower_port);
    printf("%s\n", venom);
    return 0;
}
