#include <lib.h>

int flag[256];
int filenum;
int dirnum;

void treedir(char *, int);
void tree1(char *, u_int, char *, int);

void tree(char *path) {
	int r;
    struct Stat st;
    char abs_path[512];
    if (path[0] == '/') {
        strcpy(abs_path, path);
    } else {
        getcwd(abs_path);
        pathcat(abs_path, path);
    }
    
    if ((r = stat(abs_path, &st)) < 0) {
		user_panic("stat %s: %d", abs_path, r);
    }
    
    if (!st.st_isdir) {
		printf("error opening directory %s\n", abs_path);
        printf("0 directories, 0 files\n");
        exit();
    }
	
    printf("%s\n", abs_path);
    treedir(abs_path, 0);
}

void treedir(char *path, int step) {
    int fd;
    struct File f;

    if ((fd = open(path, O_RDONLY)) < 0) {
        user_panic("open %s: %d", path, fd);
    }

    while (readn(fd, &f, sizeof(f)) == sizeof(f)) {
        if (f.f_name[0]) {
            tree1(path, f.f_type == FTYPE_DIR, f.f_name, step + 1);
        }
    }
}

void tree1(char *path, u_int isdir, char *name, int step) {
    char *sep;

    if (flag['d'] && !isdir) {
        return;
    }

    for (int i = 0; i < step - 1; i++) {
        printf("    ");
    }
    printf("|-- ");

    if (path[0] && path[strlen(path) - 1] != '/') {
		sep = "/";
	} else {
		sep = "";
	}

    if (flag['f'] && path) {
		printf("%s%s", path, sep);
    }
    printf("%s\n", name);

    if (isdir) {
        char newpath[512];
        strcpy(newpath, path);
        int namelen = strlen(name);
        int pathlen = strlen(path);

        if (strlen(sep) != 0) {
            newpath[pathlen] = '/';
            for (int i = 0; i < namelen; i++) {
                newpath[pathlen + i + 1] = name[i];
            }
            newpath[pathlen + namelen + 1] = 0;
            treedir(newpath, step);
        } else {
            for (int i = 0; i < namelen; i++) {
                newpath[pathlen + i] = name[i];
            }
            newpath[pathlen + namelen] = 0;
            treedir(newpath, step);
        }
        dirnum++;
    } else {
        filenum++;
    }
}

void usage(void) {
	printf("usage: tree [-adf] [directory...]\n");
    exit();
}

int main(int argc, char **argv) {
	int i;
    ARGBEGIN {
        default:
        	usage();
        case 'a':
        case 'd':
        case 'f':
        	flag[(u_char)ARGC()]++;
    		break;
    }
    ARGEND
        
    if (argc == 0) {
        tree("./");
        if (flag['d']) {
            printf("\n%d directories\n", dirnum);
        } else {
            printf("\n%d directories, %d files\n", dirnum, filenum);
        }
	} else {
		for (i = 0; i < argc; i++) {
            tree(argv[i]);
            if (flag['d']) {
                printf("\n%d directories\n", dirnum);
            } else {
                printf("\n%d directories, %d files\n", dirnum, filenum);
            }
		}
    }
	printf("\n");

	return 0;
}
