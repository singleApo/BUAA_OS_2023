#include <lib.h>

int main(int argc, char **argv) {
    int r;
    char buf[1024];

    if ((r = getcwd(buf)) < 0) {
        printf("error getting path: %d\n", r);
        exit();
    }
    printf("%s\n", buf);

    return 0;
}
