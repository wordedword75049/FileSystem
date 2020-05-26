#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <dirent.h>
#include <linux/unistd.h>
#include <fcntl.h>

//_syscall3(int, getdents, uint, fd, struct dirent *, dirp, uint, count);

void dump_buf(const char* buf, int len)
{
    int i;

    for (i = 0; i < len; ++i) {
        if (i % 16 == 0)
            printf("0x%x: ", i);

        printf("%c, ", buf[i]);

        if (i % 16 == 15)
            printf("\n");
    }
}

int main(int argc, char *argv[])
{
        char buf[0x400];
        struct dirent *dirent;
        int rc;
        int fd = open(argv[1], O_RDONLY | O_NONBLOCK | O_DIRECTORY);
        if (fd == -1) {
                perror("open:");
                exit(1);
        }

        rc = getdents(fd, (struct dirent*)buf, 0x400);
        if (rc == -1) {
                perror("getdents:");
                exit(1);
        }

        /*dump_buf(buf, 0x400);*/

        printf("resulting bytes - %d\n", rc);
        dirent = (struct dirent*)buf;
        while ( (((char*)dirent) - buf) < rc) {
                printf("entry: ino=%ld, offset=%lu, reclen=%hu, name='%s'\n",
                       dirent->d_ino, dirent->d_off, dirent->d_reclen, dirent->d_name);
                dirent = (struct dirent*)(((char*)dirent) + dirent->d_reclen);
                printf("((char*)dirent) - buf = %d\n", ((char*)dirent) - buf);
        }

        close(fd);

        return 0;
}
