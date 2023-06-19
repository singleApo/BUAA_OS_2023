#include <lib.h>

int chdir(char *path) {
    return syscall_set_path(path);
}

int getcwd(char *buf) {
    return syscall_get_path(buf);
}

void pathcat(char *path, const char *suffix) {
    int length = strlen(path);
	if (suffix[0] == '.') {
		suffix += 2;    // skip "./"
	}
	int suf_len = strlen(suffix);

	if (length != 1) {
		path[length++] = '/';
	}
	for (int i = 0; i < suf_len; i++) {
		path[length++] = suffix[i];
	}
	path[length] = 0;
}
