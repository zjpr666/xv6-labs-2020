#include "user.h"
#include "kernel/types.h"

int main(int argc, char* const argv[])
{
    if(argc != 2)
    {
        fprintf(2,"usage: sleep <time>\n");
        exit(1);
    }
    sleep(atoi(argv[1]));
    exit(0);
}