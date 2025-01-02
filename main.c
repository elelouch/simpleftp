#include <err.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define ARR_SIZE(arr) sizeof(arr)/sizeof(arr[0])

struct pedro {
    int hola;
    int chau;
};
#define HOLA " "

int main(int argc, char *argv[])
{
    char buffer[256];
    char hola[] = "eliasrojas\n";
    // write(STDOUT_FILENO, hola, sizeof(hola));
    printf("%c\n", getopt(argc, argv, "A"));

    return 0;
}
