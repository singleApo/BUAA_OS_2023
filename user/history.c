#include <lib.h>

int main(int argc, char **argv) {
    int r;
    int fd;
    int line = 1;
    int newline = 1;
    char buf;

    if ((fd = open("/.history", O_RDONLY)) < 0) {
        user_panic("error opening .history: %d\n", fd);
    }

    while ((r = read(fd, &buf, 1)) == 1) {
        if (newline) {
            printf("%2d  ", line);
            newline = 0;
        } 
        printf("%c", buf);

        if (buf == '\n') {
            newline = 1;
            line++;
        }
    }

    return 0;
}