#include <lib.h>

void usage(void) 
{
	printf("usage: mkdir [dirname]\n");
    exit();
}

int main(int argc, char **argv) {
    int i;
    int r;

    if (argc == 1) { // no dirname
        usage();
    } else {
        for (i = 1; i < argc; i++) {
            if ((r = open(argv[i], O_RDONLY)) >= 0) {
                user_panic("directory %s exists", argv[i]);
            } 
            if ((r = create(argv[1], FTYPE_DIR)) < 0) {
                user_panic("error creating directory %s: %d\n", argv[i], r);
            }
        }
    }
    return 0;
}
