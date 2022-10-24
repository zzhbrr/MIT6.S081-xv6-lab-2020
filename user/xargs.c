#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int
main(int argc, char *argv[])
{
    if (argc <= 1) {
        fprintf(1, "error: 'xargs {command}'\n");
        exit(1);
    }
    char buf[512];
    int End = 0;
    while(1) {
        int exec_argc = argc - 1;
        char *exec_argv[32];
        for (int i = 1; i < argc; i ++) exec_argv[i-1] = argv[i];
        int cnt = 0, last_end = 0;
        while(1) {
            read(0, &buf[cnt], 1);
            if (*(buf+cnt) == '\n' || *(buf+cnt) == '\0') {
                if (*(buf+cnt) == '\0') {End = 1;break;}
                *(buf+cnt) = '\0';
                exec_argv[exec_argc] = buf+last_end;
                exec_argc++;
                break;
            }
            if (*(buf+cnt) == ' ') {
                *(buf+cnt) = '\0';
                exec_argv[exec_argc] = buf+last_end;
                exec_argc++;
                last_end = cnt+1;
            }
            cnt++;
        }
        if (End == 1) {break;}
        // fprintf(1, "cnt = %d\n", cnt);
        if (fork() == 0){
            // fprintf(1, "run %s with %d arags :", exec_argv[0], exec_argc);
            // for (int i = 1; i < exec_argc; i ++) fprintf(1, "%s_", exec_argv[i]);
            // fprintf(1, "\n");
            exec(exec_argv[0], exec_argv);
            exit(0); 
        } else {wait(0);}
        
    }
    exit(0);
}