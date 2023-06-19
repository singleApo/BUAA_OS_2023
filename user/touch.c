#include <lib.h>

void usage(void) 
{
	printf("usage: touch [filename]\n");
    exit();
}

int main(int argc, char **argv) {
    int i;
    int r;

    if (argc == 1) { // no filename
        usage();
    } else {
        for (i = 1; i < argc; i++) {
            if ((r = open(argv[i], O_RDONLY)) >= 0) {
                user_panic("file %s exists", argv[i]);
            } 
            if ((r = create(argv[1], FTYPE_REG)) < 0) {
                user_panic("error creating file %s: %d\n", argv[i], r);
            }
        }
    }
    return 0;
}