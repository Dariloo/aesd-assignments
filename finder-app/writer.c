#include <stdio.h>

int main(int argc, char *argv[])
{
    FILE *file;

    if (argc != 3)
    {
        return 1;
    }

    file = fopen(argv[1], "w");

    if (file == NULL)
    {
        return 1;
    }

    fputs(argv[2], file);

    fclose(file);

    return 0;
}
