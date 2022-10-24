#include "kernel/types.h"
#include "user/user.h"

int p[20][2];

void 
children_processes(int pipe_cnt) {
    int *ptr, prime, i;
    ptr = &prime;
    read(p[pipe_cnt-1][0], ptr, 4);
    fprintf(1, "prime %d\n", *ptr);
    ptr = &i;
    int need_new_process = 0;
    while(read(p[pipe_cnt-1][0], ptr, 4)) {
        if ((*ptr) % prime != 0){
            if (need_new_process == 0) {
                pipe(p[pipe_cnt]);
                need_new_process = 1;
            }
            write(p[pipe_cnt][1], ptr, 4);
            
        }
    }
    close(p[pipe_cnt-1][0]);
    close(p[pipe_cnt][1]);
    if (need_new_process == 1) {
        children_processes(pipe_cnt+1);
    }
    wait((int*)0);
}


int
main(int argc, char *argv[]) 
{
    int pipe_cnt = 0;

    pipe(p[0]);
    pipe_cnt ++;
    int *ptr;
    for (int i = 2; i <= 35; i ++) {
        ptr = &i;
        write(p[0][1], (void*)ptr, 4);
    }
    close(p[0][1]);
    children_processes(pipe_cnt);
    wait((int *)0);
    exit(0);
}