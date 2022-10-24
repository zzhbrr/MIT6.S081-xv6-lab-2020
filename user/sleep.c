#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[]) 
{
    if (argc <= 1) {
        fprintf(2, "error: please add time after 'sleep'\n");
        exit(1);
    }
    int t = atoi(argv[1]);
    
    sleep(t);
    exit(0);
}