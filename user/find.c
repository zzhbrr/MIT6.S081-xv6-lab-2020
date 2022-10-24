#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
    char *p;

    // Find first character after last slash.
    for(p=path+strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;
    return p;
}

void 
Search(char *path, char *file_name) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        // fprintf(2, "file find : %s\n", fmtname(path));
        // fprintf(2, "file_name = %s\n", file_name);
        // fprintf(2, "path_name = %s\n", fmtname(path));
        // fprintf(2, "whether is the same: %d\n", strcmp(file_name, fmtname(path)));
        if (strcmp(file_name, fmtname(path)) == 0)
            fprintf(1, "%s\n", path);
        break;
    case T_DIR:
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
            printf("find: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0)
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if (strcmp(de.name, ".") == 0) continue;
            if (strcmp(de.name, "..") == 0) continue;
            if(stat(buf, &st) < 0){
                printf("find: cannot stat %s\n", buf);
                continue;
            }
            // fprintf(2, "searching on %s\n", buf);
            Search(buf, file_name);
        }
        break;
    default:
        break;
    }
    close(fd);

}

int
main(int argc, char *argv[]) 
{
    if (argc <= 2) {
        fprintf(2, "error: 'find {root directory} {file name}'\n");
        exit(1);
    }
    Search(argv[1], argv[2]);
    exit(0);
}