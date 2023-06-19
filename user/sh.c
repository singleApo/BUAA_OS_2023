#include <args.h>
#include <lib.h>

#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"

#define MOVEUP(y) printf("\033[%dA", (y))
#define MOVEDOWN(y) printf("\033[%dB", (y))
#define MOVELEFT(y) printf("\033[%dD", (y))
#define MOVERIGHT(y) printf("\033[%dC",(y))

static int history_init;
static int linenum;
static int offset[1024];

/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2) {
	*p1 = 0;
	*p2 = 0;
	if (s == 0) {
		return 0;
	}

	while (strchr(WHITESPACE, *s)) {
		*s++ = 0;
	}
	if (*s == 0) {
		return 0;
	}

	if (*s == '"') {
		s++;
		*p1 = s;
		while (*s && *s != '"') {
			s++;
		}
		*p2 = s;
		if (*s == 0) {
			return 0;
		}
		*s++ = 0;
		return 'w';
	}

	if (strchr(SYMBOLS, *s)) {
		int t = *s;
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		return t;
	}

	*p1 = s;
	while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
		s++;
	}
	*p2 = s;
	return 'w';
}

int gettoken(char *s, char **p1) {
	static int c, nc;
	static char *np1, *np2;

	if (s) {
		nc = _gettoken(s, &np1, &np2);
		return 0;
	}
	c = nc;
	*p1 = np1;
	nc = _gettoken(np2, &np1, &np2);
	return c;
}

void getcmd(int line, char *cmd) {
	int r;
	int fd;
	char temp[4096];
	int cmdlen;

	if ((fd = open("/.history", O_RDONLY)) < 0) {
		debugf("error opening .history: %d\n", fd);
		exit();
	}

	if (line != 0) {
		if ((r = readn(fd, temp, offset[line - 1])) != offset[line - 1]) {
			user_panic("error reading .history: %d", r);
		}
		cmdlen = offset[line] - offset[line - 1];
	} else {
		cmdlen = offset[line];
	}

	if ((r = readn(fd, cmd, cmdlen)) != cmdlen) {
		user_panic("error reading .history: %d", r);
	}
	cmd[cmdlen - 1] = 0;

	close(fd);
}

#define MAXARGS 128

int parsecmd(char **argv, int *rightpipe) {
	int argc = 0;

	while (1) {
		char *t;
		int fd, r;
		int c = gettoken(0, &t);
		switch (c) {
			case 0:
				return argc;
			case 'w':
				if (argc >= MAXARGS) {
					debugf("too many arguments\n");
					exit();
				}
				argv[argc++] = t;
				break;

			case '<':
				if (gettoken(0, &t) != 'w') {
					debugf("syntax error: < not followed by word\n");
					exit();
				}
				// Open 't' for reading, dup it onto fd 0, and then close the original fd.
				/* Exercise 6.5: Your code here. (1/3) */
				fd = open(t, O_RDONLY);
				dup(fd, 0);
				close(fd);
				break;

			case '>':
				if (gettoken(0, &t) != 'w') {
					debugf("syntax error: > not followed by word\n");
					exit();
				}
				// Open 't' for writing, dup it onto fd 1, and then close the original fd.
				/* Exercise 6.5: Your code here. (2/3) */
				if ((fd = open(t, O_WRONLY)) < 0) {
					if ((r = create(t, FTYPE_REG)) < 0) {
						debugf("error create file %s: %d\n", t, r);
						exit();
					}
					fd = open(t, O_WRONLY);
				}
				dup(fd, 1);
				close(fd);
				break;

			case '|':;
				/*
				* First, allocate a pipe.
				* Then fork, set '*rightpipe' to the returned child envid or zero.
				* The child runs the right side of the pipe:
				* - dup the read end of the pipe onto 0
				* - close the read end of the pipe
				* - close the write end of the pipe
				* - and 'return parsecmd(argv, rightpipe)' again, to parse the rest of the
				*   command line.
				* The parent runs the left side of the pipe:
				* - dup the write end of the pipe onto 1
				* - close the write end of the pipe
				* - close the read end of the pipe
				* - and 'return argc', to execute the left of the pipeline.
				*/
				int p[2];
				/* Exercise 6.5: Your code here. (3/3) */
				pipe(p);
				if ((*rightpipe = fork()) == 0) {	// child
					dup(p[0], 0);
					close(p[0]);
					close(p[1]);
					return parsecmd(argv, rightpipe);
				} else {							// parent
					dup(p[1], 1);
					close(p[0]);
					close(p[1]);
					return argc;
				}
				break;

			case ';':
				if ((*rightpipe = fork()) == 0) {	// child
					return argc;
				} else {							// parent
					wait(*rightpipe);
					return parsecmd(argv, rightpipe);
				}
				break;

			case '&':
				if ((r = fork()) == 0) {			// child
					return argc;
				} else {							// parent
					return parsecmd(argv, rightpipe);
				}
				break;
		}
	}

	return argc;
}

void runcmd(char *s) {
	gettoken(s, 0);

	char *argv[MAXARGS];
	int rightpipe = 0;
	int argc = parsecmd(argv, &rightpipe);
	if (argc == 0) {
		return;
	}
	argv[argc] = 0;

	if (strcmp(argv[0], "cd") == 0) {
		int r;
		char path[512];
		if (argc == 1) {
			r = chdir("/");
			printf("back to the root directory\n");
			return;
		} else if (argv[1][0] == '/') { // absolute path
			strcpy(path, argv[1]);	
		} else {         	  			// relative path
			getcwd(path);
			pathcat(path, argv[1]);
		}

		if ((r = open(path, O_RDONLY)) < 0) {
			printf("error opening path %s: %d\n", path, r);
			exit();
		}
		close(r);
		struct Stat st;
		if ((r = stat(path, &st)) < 0) {
			user_panic("stat %s: %d", path, r);
		}
		if (!st.st_isdir) {
			printf("path %s is not a directory\n", path);
			exit();
		}

		if ((r = chdir(path)) < 0) {
			printf("cd failed: %d\n", r);
			exit();
		}
		return;
	}

	int child = spawn(argv[0], argv);
	close_all();
	if (child >= 0) {
		wait(child);
	} else {
		debugf("spawn %s: %d\n", argv[0], child);
	}
	if (rightpipe) {
		wait(rightpipe);
	}
	exit();
}

int direction() {
	int r;
	char temp1, temp2;
	if ((r = read(0, &temp1, 1)) != 1) {
		if (r < 0) {
			debugf("read error: %d\n", r);
		}
		exit();
	}
	if ((r = read(0, &temp2, 1)) != 1) {
		if (r < 0) {
			debugf("read error: %d\n", r);
		}
		exit();
	}
	if (temp1 == '[') {
		switch (temp2) {
			case 'A': 	// up:\033[A
				return 1;
			case 'B': 	// down:\033[B
				return 2;
			case 'D':	// left:\033[D
				return 3;
			case 'C': 	// right:\033[C
				return 4;
			default:
				return 0;
		}
	}
	return 0;
}

void readline(char *buf, u_int n) {
	int r;
	int length = 0; // actual length 
	int i = 0;   	// cursor position
	char temp;
	int cmdline = linenum;
	char curcmd[1024];

	while (i < n) {
		if ((r = read(0, &temp, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit();
		}

		if (temp == '\b' || temp == 0x7f) {	// backspace
			if (i > 0) {
				if (i == length) {	// cursor at the end
					buf[length - 1] = 0;
					MOVELEFT(i);
					printf("%s ", buf);
					MOVELEFT(1);
				} else { 			// cursor in the middle
					for (int j = i - 1; j < length - 1; j++) {
						buf[j] = buf[j + 1]; // shift left
					}
					buf[length - 1] = 0;
					MOVELEFT(i);
					printf("%s ", buf);
					MOVELEFT(length - i + 1);
				}
				i--;
				length--;
			}
		} else if (temp == '\033') {
			switch (direction())
			{
				case 1: 			// up
					MOVEDOWN(1);
					if (cmdline == linenum) {
						buf[length] = 0;
						strcpy(curcmd, buf); // save current command
					}
					if (cmdline > 0) {
						cmdline--;
					} else {
						break;
					}
					getcmd(cmdline, buf);
					if (i > 0) {
						MOVELEFT(i);
					}
					printf("%s", buf);

					if (strlen(buf) < length) {
						for (int j = 0; j < length - strlen(buf); j++) {
							printf(" ");
						}
						MOVELEFT(length - strlen(buf));
					}

					length = strlen(buf);
					i = length;
					break;

				case 2: 			// down
					if (cmdline == linenum - 1) {
						strcpy(buf, curcmd); // restore current command
					}
					if (cmdline < linenum) {
						cmdline++;
					} else {
						break;
					} 
					getcmd(cmdline, buf);
					if (i > 0) {
						MOVELEFT(i);
					}
					printf("%s", buf);

					if (strlen(buf) < length) {
						for (int j = 0; j < length - strlen(buf); j++) {
							printf(" ");
						}
						MOVELEFT(length - strlen(buf));
					}

					length = strlen(buf);
					i = length;
					break;
					
				case 3: 			// left
					if (i > 0) {
						i--;
					} else {
						MOVERIGHT(1);
					}
					break;
				case 4: 			// right
					if (i < length) {
						i++;
					} else {
						MOVELEFT(1);
					}
					break;
				default: 
					break;
			}
		} else if (temp == '\r' || temp == '\n') {
			buf[length] = 0;
			return;
		} else {				// visible character
			if (i == length) { 	// cursor at the end
				buf[i] = temp;
			} else {			// cursor in the middle
				for (int j = length; j > i; j--) {
					buf[j] = buf[j - 1]; // shift right
				}
				buf[i] = temp;
				buf[length + 1] = 0;
				MOVELEFT(i + 1);
				printf("%s", buf);
				MOVELEFT(length - i);
			}
			i++;
			length++;
		}
        
        if (length == n) {
			break;
		}
	}
	debugf("line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

char buf[1024];

void savecmd(const char *s) {
	int r;
	int fd;

	if (!history_init) {
		if ((r = create("/.history", FTYPE_REG)) < 0) {
			debugf("error creating .history\n");
			exit();
		}
		history_init = 1;
	}

	if ((fd = open("/.history", O_WRONLY | O_APPEND)) < 0) {
		debugf("error opening .history: %d\n", r);
		exit();
	}

	if ((r = write(fd, s, strlen(s))) != strlen(s)) {
		user_panic("error writng .history: %d\n", r);
	}
	write(fd, "\n", 1);

	if (linenum == 0) {
		offset[linenum] = strlen(s) + 1;
	} else {
		offset[linenum] = offset[linenum - 1] + strlen(s) + 1;
	}
	linenum++;

	close(fd);
}

void usage(void) {
	debugf("usage: sh [-dix] [command-file]\n");
	exit();
}

int main(int argc, char **argv) {
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
	debugf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	debugf("::                                                         ::\n");
	debugf("::                                                         ::\n");
	debugf("::                     MOS Shell 2023                      ::\n");
	debugf("::                 completed by singleApo                  ::\n");
	debugf("::                                                         ::\n");
	debugf("::                                                         ::\n");
	debugf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	
	if ((r = chdir("/")) < 0) {
		printf("error creating root path: %d\n", r);
	}

	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		close(0);
		if ((r = open(argv[1], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[1], r);
		}
		user_assert(r == 0);
	}
	for (;;) {
		if (interactive) {
			printf("\n$ ");
		}
		readline(buf, sizeof buf);
		savecmd(buf);

		if (buf[0] == '#') {
			continue;
		}
		if (echocmds) {
			printf("# %s\n", buf);
		}
		if ((r = fork()) < 0) {
			user_panic("fork: %d", r);
		}
		if (r == 0) {
			runcmd(buf);
			exit();
		} else {
			wait(r);
		}
	}
	return 0;
}
