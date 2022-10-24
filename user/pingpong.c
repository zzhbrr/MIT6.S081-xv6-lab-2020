#include "kernel/types.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
    int p1[2], p2[2]; // p1: child->parent, p2: parent->child
    pipe(p1), pipe(p2);
    char s[10];
    if (fork() == 0) {
        int pid = getpid();
        close(p1[0]);
        close(p2[1]);
        read(p2[0], s, 5);
        fprintf(1, "%d: received ping\n", pid);
        write(p1[1], "ping", 5);
        close(p1[1]);
        close(p2[0]);
    } else {
        int pid = getpid();
        close(p1[1]);
        close(p2[0]);
        write(p2[1], "ping", 5);
        read(p1[0], s, 5);
        fprintf(1, "%d: received pong\n", pid);
        close(p1[0]);
        close(p2[1]);
    }
    exit(0);
}