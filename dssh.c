// Steven Rakhmanchik and Derrick Lin
// System Level Programming 2021
// DSSH Shell
//
// Built In Commands:
// -> help			<- Displays help menu with all built-in commands
// -> cd				<- Changes hirectory
// -> history		<- Displays shell command history
// -> issue			<- Reruns command that was run a specified number of commands ago
// -> ls				<- Prints files in current directory
// -> rm				<- Removes directory, or file
// -> rmexcept 	<- Removes all files except those specified
// -> exit			<- Exits DSSH shell

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <ftw.h>
#include <limits.h>
#include<sys/utsname.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#ifdef _WIN32
    printf("You are on windows. This will not work.");
		exit();
#endif

#ifndef MAX_BUF
#define MAX_BUF 200
#endif

static char ** commands;
static int commandCount=0;
int sh_cd(char **args);
int sh_help(char **args);
int sh_history(char** args);
int sh_issue(char **args);
int sh_rm(char **args);
int sh_ls(char **args);
int sh_rmexcept(char **args);
int sh_exit(char **args);

int sh_num_builtins();
char **sh_split_line(char *line);
char *sh_read_line(void);
int sh_launch(char **args);
int sh_execute(char **args);
void sh_loop(void);

char *builtin_str[] = {
	"help",
	"cd",
	"history",
	"issue",
	"ls",
	"rm",
	"rmexcept",
	"exit"
};

int (*builtin_func[]) (char **) = {
	&sh_help,
	&sh_cd,
	&sh_history,
	&sh_issue,
	&sh_ls,
	&sh_rm,
	&sh_rmexcept,
	&sh_exit
};

int sh_num_builtins() {
	return sizeof(builtin_str) / sizeof(char *);
}

int sh_help(char **args) {
	int i;
	printf("\nDSHH Help Information:\n", KMAG);
	printf("help				<- Displays help menu with all built-in commands\n", KCYN, KWHT);
	printf("cd					<- Changes hirectory\n", KCYN, KWHT);
	printf("history			<- Displays shell command history\n", KCYN, KWHT);
	printf("issue				<- Reruns command that was run a specified number of commands ago\n", KCYN, KWHT);
	printf("ls					<- Prints files in current directory\n", KCYN, KWHT);
	printf("rm					<- Removes directory, or file\n", KCYN, KWHT);
	printf("rmexcept		<- Removes all files except those specified\n", KCYN, KWHT);
	printf("exit				<- Exits DSSH shell\n", KCYN, KWHT);
	return 1;
}

int sh_cd(char **args) {
	if (args[1] == NULL) {
		fprintf(stderr, "DSSH: expected argument to \"cd\"\n");
	} else {
		if (chdir(args[1]) != 0) {
			perror("sh");
		}
	}
	return 1;
}

int sh_history(char**args) {
	int j,i;
	if (args[1] == NULL) {
		i = 0;
	} else {
		i = commandCount - atoi(args[1]);
		if(i<0)i=0;
	}
	for(j = i; j < commandCount; j++)
		printf("\t%d\t%s\n", j+1, commands[j]);
	return 1;
}

int sh_issue(char **args) {
	if (args[1] == NULL) {
		fprintf(stderr, "DSSH: expected argument to \"issue\"\n");
	} else {
		int n = atoi(args[1]);
		if(n <= 0 || n > commandCount){
			fprintf(stderr, "DSSH: invalid argument to \"issue\". Argument out of range\n");
			return 1;
		}

		n--;
		printf("%s\n\n", commands[n]);
		char **commandargs = sh_split_line(commands[n]);
		int status = sh_execute(commandargs);
		free(commandargs);
		return status;
	}
	return 1;
}

int sh_ls(char **args) {
	struct dirent **namelist;
	int n = scandir(".", &namelist, NULL, alphasort);
	if(n < 0)
	{
		perror("sh");
	}
	else
	{
		int i;
		i = 0;
		while (n--)
		{
			i++;
			printf("%s\n", namelist[n]->d_name);
			free(namelist[n]);
		}
		free(namelist);
	}
	return 1;
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
	int rv = remove(fpath);

	if (rv)
		perror(fpath);

	return rv;
}

int unlink_cb_verb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
	int rv = remove(fpath);
	if (rv)
		perror(fpath);
	else	printf("%s\n", fpath);
	return rv;
 }

int sh_rm(char **args) {
	int argc = 1;
	bool RECURSIVE_FLAG = false,
		VERBOSE_FLAG = false,
		FORCE_FLAG = false;
	char filename[2048] = "";

	while(args[argc] != NULL)
	{
		if (strcmp(args[argc], "-r") == 0) {
			RECURSIVE_FLAG = true;
		} else if (strcmp(args[argc], "-f") == 0) {
			FORCE_FLAG = true;
		} else if (strcmp(args[argc], "-v") == 0) {
			VERBOSE_FLAG = true;
		} else {
			if (args[argc][0] != '/') {
				getcwd(filename, sizeof(filename));
				strcat(filename, "/");
			}
			strcat(filename, args[argc]);
		}
		argc++;
	}

	if (filename == NULL) {
		fprintf(stderr, "DSSH: expected a file name to \"rm\"\n");
	} else if (!RECURSIVE_FLAG) {
		if (unlink(filename) != 0) {
			perror("sh");
			return 1;
		}
		if (VERBOSE_FLAG)	printf("%s\n", filename);
	} else {
		if(VERBOSE_FLAG)	nftw(filename, unlink_cb_verb, 64, FTW_DEPTH | FTW_PHYS);
		else	nftw(filename, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
	}
	return 1;
}

int sh_rmexcept(char **args) {
	int j, i = 0, flag = 0;

	while(args[i] != NULL)	i++;

	DIR *d;
	struct dirent *dir;

	d = opendir(".");
	if (d) {
		while ((dir = readdir(d)) != NULL) {

			if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
				continue;
			}

			for(j = 1; j < i; j++) {
				if( strcmp(args[j], dir->d_name) == 0 ) {
					flag = 0;
					break;
				} else {
					flag = 1;
				}
			}

			if (flag == 1) {
				char filename[2048] = "";
				getcwd(filename, sizeof(filename));
				strcat(filename, "/");
				strcat(filename, dir->d_name);
				if (unlink(filename) != 0) {
					perror("sh");
				}
				printf("%s\n", filename);
			}
		}

		closedir(d);
	}
	return 1;
}

int sh_exit(char **args) {
	return 0;
}

static void sig_handler(int signo) {
}

int sh_launch(char **args) {
	pid_t pid, wpid;
	int status;

	pid = fork();
	if (pid == 0) {

		int argc = 1;
		bool KILL_FLAG = false;
		int killTime = 0;

		while(args[argc] != NULL) {
			if(strcmp(args[argc], "--tkill") == 0) {
				KILL_FLAG = true;
				killTime = (args[argc+1] == NULL) ? 0 : atoi(args[argc+1]);
				break;
			}
			argc++;
		}

		if (KILL_FLAG) {
			signal(SIGALRM, sig_handler);
			alarm(killTime);
		}

		if (execvp(args[0], args) == -1) {
			perror("sh");
		}
		exit(EXIT_FAILURE);
	} else if (pid < 0) {
		perror("sh");
	} else {
		do {
			wpid = waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}

	return 1;
}

int sh_execute(char **args) {
	int i;

	if (args[0] == NULL) {
		return 1;
	}

	if (strcmp(args[0], "cd") == 0) {
		return (*builtin_func[1])(args);
	} else if (strcmp(args[0], "rm") == 0) {
		return (*builtin_func[5])(args);
	} else if (strcmp(args[0], "rmexcept") == 0) {
		return (*builtin_func[6])(args);
	}
	for (i = 0; i < sh_num_builtins(); i++) {
		if (strcmp(args[0], builtin_str[i]) == 0) {

			pid_t pid, wpid;
			int status;

			pid = fork();

			if (pid == 0) {
				status = (*builtin_func[i])(args);
				exit(status);
			} else if (pid < 0) {
				perror("sh");
			} else {
				waitpid(-1, &status, 0);
				return WEXITSTATUS(status);
			}
		}
	}
	return sh_launch(args);
}
#define sh_TOK_BUFSIZE 64
#define sh_TOK_DELIM " \t\r\n\a"
char **sh_split_line(char *line) {
	int bufsize = sh_TOK_BUFSIZE, position = 0;
	char **tokens = malloc(bufsize * sizeof(char*));
	char *token;

	if (!tokens) {
		fprintf(stderr, "DSSH: allocation error\n");
		exit(EXIT_FAILURE);
	}

	token = strtok(line, sh_TOK_DELIM);
	while (token != NULL) {
		tokens[position] = token;
		position++;

		if (position >= bufsize) {
			bufsize += sh_TOK_BUFSIZE;
			tokens = realloc(tokens, bufsize * sizeof(char*));
			if (!tokens) {
				fprintf(stderr, "DSSH: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}

		token = strtok(NULL, sh_TOK_DELIM);
	}
	tokens[position] = NULL;
	return tokens;
}

#define sh_RL_BUFSIZE 1024
char *sh_read_line(void) {
	int bufsize = sh_RL_BUFSIZE;
	int position = 0;
	char *buffer = malloc(sizeof(char) * bufsize);
	int c;

	if (!buffer) {
		fprintf(stderr, "DSSH: allocation error\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		c = getchar();
		if (c == EOF || c == '\n') {
			buffer[position] = '\0';
			return buffer;
		} else {
			buffer[position] = c;
		}
		position++;
		if (position >= bufsize) {
			bufsize += sh_RL_BUFSIZE;
			buffer = realloc(buffer, bufsize);
			if (!buffer) {
				fprintf(stderr, "DSSH: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}

#define sh_HIST_BUFFER 10
void sh_loop(void) {
	char *line;
	char **args;
	int status;
	int histSize = sh_HIST_BUFFER;
	commands = (char **)malloc(sizeof(char *)*histSize);
	commandCount = 0;
	char *command;
	do {
		int getlogin_r(char *buf, size_t bufsize);
		char path[MAX_BUF];
    getcwd(path, MAX_BUF);
		char hostname[1024];
		char username[1024];
		gethostname(hostname, 1024);
		getlogin_r(username, 1024);

		printf("%s%s@%s %s%s %s$ ", KGRN, username, hostname, KCYN, path, KWHT);

		command = sh_read_line();
		commands[commandCount] = (char *)malloc(sizeof(*command));
		strcpy(commands[commandCount], command);
		args = sh_split_line(command);
		status = sh_execute(args);
		free(args);
		free(command);
		commandCount++;

		if(commandCount >= histSize){
			histSize += sh_HIST_BUFFER;
			commands = realloc(commands, sizeof(char *) * histSize);
			if(!commands){
				fprintf(stderr, "%s\n", "DSSH: allocation error");
				exit(EXIT_FAILURE);
			}
		}
	} while (status);
}

int main(int argc, char **argv)
{
	system("clear");
	struct utsname uts;
	uname(&uts);
	printf("%s%s %s %son %s%s\n", KCYN, uts.sysname, uts.release, KWHT, KCYN, uts.machine);
	printf("%sDSSH Shell %sBy %sSteven Rakhmanchik %sand %sDerrick Lin\n\n", KCYN, KWHT, KCYN, KWHT, KCYN);
	sh_loop();

	return EXIT_SUCCESS;
}
