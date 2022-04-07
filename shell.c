#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>

#define BUFFER_SIZE 256
#define TRUE 1
#define FALSE 0

/*some symbol string*/
const char* COMMAND_EXIT = "exit";
const char* COMMAND_HELP = "help";
const char* COMMAND_CD = "cd";
const char* COMMAND_IN = "<";
const char* COMMAND_OUT = ">";
const char* COMMAND_PIPE = "|";

// enum the error type, so that we don't have to define it one by one
enum {
	RESULT_NORMAL,
	ERROR_FORK,
	ERROR_COMMAND,
	ERROR_WRONG_PARAMETER,
	ERROR_MISS_PARAMETER,
	ERROR_TOO_MANY_PARAMETER,
	ERROR_CD,
	ERROR_SYSTEM,
	ERROR_EXIT,

	/* error types about redirect*/
	ERROR_MANY_IN,
	ERROR_MANY_OUT,
	ERROR_FILE_NOT_EXIST,
	
	/* error types about pipe */
	ERROR_PIPE,
	ERROR_PIPE_MISS_PARAMETER
};

/*define curPath and commands as global variables*/
char curPath[BUFFER_SIZE];
char commands[BUFFER_SIZE][BUFFER_SIZE];

int isCommandExist(const char* command);
int getCurWorkDir();
int splitCommands(char command[BUFFER_SIZE]);
int conduct_Exit();
int conduct_cmd(int commandNum);
int conduct_help();
int conduct_pipe(int left, int right);
int conduct_redirect(int left, int right);
int conduct_Cd(int commandNum);

int main() {
	/*get curWorkDir*/
	if (ERROR_SYSTEM == getCurWorkDir()) {
		fprintf(stderr, "\e[31;1mError: System error while getting current work directory.\n\e[0m");
		exit(ERROR_SYSTEM);
	}

	/* begin the shell */
	char inputBuffer[BUFFER_SIZE];
	while (TRUE) {
        printf("\e[32;1m[Senjougahara]:%s\e[0m$ ", curPath); 
		/* input*/
		fgets(inputBuffer, BUFFER_SIZE, stdin);
		int len = strlen(inputBuffer);
		if (len != BUFFER_SIZE) {
			inputBuffer[len-1] = '\0';
		}

		int commandNum = splitCommands(inputBuffer);

        /*test commands*/
        // for(int i=0; i<commandNum; i++){
        //     printf("%s\n", commands[i]);
        // }
		
		if (commandNum != 0) {
			/*internal cmd*/
			if (strcmp(commands[0], COMMAND_EXIT) == 0) { //exit
				if (ERROR_EXIT == conduct_Exit()) 
					exit(-1);
			} else if (strcmp(commands[0], COMMAND_CD) == 0) { // cd
				switch (conduct_Cd(commandNum)) {
					case ERROR_MISS_PARAMETER:
						fprintf(stderr, "\e[31;1mError: Miss parameter while using command \"%s\".\n\e[0m", COMMAND_CD);
						break;
					case ERROR_WRONG_PARAMETER:
						fprintf(stderr, "\e[31;1mError: No such path \"%s\".\n\e[0m", commands[1]);
						break;
					case ERROR_TOO_MANY_PARAMETER:
						fprintf(stderr, "\e[31;1mError: Too many parameters while using command \"%s\".\n\e[0m", COMMAND_CD);
						break;
					case RESULT_NORMAL: 
						if (ERROR_SYSTEM == getCurWorkDir()) {
							fprintf(stderr, "\e[31;1mError: System error while getting current work directory.\n\e[0m");
							exit(ERROR_SYSTEM);
						} else break;
				}
			} else if (strcmp(commands[0], COMMAND_HELP) == 0){// help
				conduct_help();
			} else { // external cmd
				switch (conduct_cmd(commandNum)) {
					case ERROR_FORK:
						fprintf(stderr, "\e[31;1mError: Fork error.\n\e[0m");
						exit(ERROR_FORK);
					case ERROR_COMMAND:
						fprintf(stderr, "\e[31;1mError: Command not exist in myshell.\n\e[0m");
						break;
					case ERROR_MANY_IN:
						fprintf(stderr, "\e[31;1mError: Too many redirection symbol \"%s\".\n\e[0m", COMMAND_IN);
						break;
					case ERROR_MANY_OUT:
						fprintf(stderr, "\e[31;1mError: Too many redirection symbol \"%s\".\n\e[0m", COMMAND_OUT);
						break;
					case ERROR_FILE_NOT_EXIST:
						fprintf(stderr, "\e[31;1mError: Input redirection file not exist.\n\e[0m");
						break;
					case ERROR_MISS_PARAMETER:
						fprintf(stderr, "\e[31;1mError: Miss redirect file parameters.\n\e[0m");
						break;
					case ERROR_PIPE:
						fprintf(stderr, "\e[31;1mError: Open pipe error.\n\e[0m");
						break;
					case ERROR_PIPE_MISS_PARAMETER:
						fprintf(stderr, "\e[31;1mError: Miss pipe parameters.\n\e[0m");
						break;
				}
			}
		}
	}
}

/**
 * @brief check whether the cmd exits or not by cmd "command -v xxx"
 * 
 * @param command const command buffer
 * @return : int error type
 */
int isCommandExist(const char* command) { 
	if (command == NULL || strlen(command) == 0) return FALSE;
	int result = TRUE;
    /*create pipe, fd[0]--read port, fd[1]--write port*/
	int fd[2];
	if (pipe(fd) == -1) {
		result = FALSE;
	} else {
		/* deposit the STDIN and STDOUT, 
        since we will use redirect skill in function */
		int inFd = dup(STDIN_FILENO);
		int outFd = dup(STDOUT_FILENO);

		pid_t pid = vfork();
		if (pid == -1) {
			result = FALSE;
		} else if (pid == 0) {
			/*the output of next cmds will be put in fd[1]*/
			close(fd[0]);
			dup2(fd[1], STDOUT_FILENO);
			close(fd[1]);

			char tmp[BUFFER_SIZE];
			sprintf(tmp, "command -v %s", command);// create string
			system(tmp);
			exit(1);
		} else {
			waitpid(pid, NULL, 0);
			/* input of next cmds will be fd[0] */
			close(fd[1]);
			dup2(fd[0], STDIN_FILENO);
			close(fd[0]);

			if (getchar() == EOF) { // without data, which means commands is not exit
				result = FALSE;
			}
			
			/*restore stdin and stdout*/
			dup2(inFd, STDIN_FILENO);
			dup2(outFd, STDOUT_FILENO);
		}
	}

	return result;
}
/**
 * @brief Get the Cur Work Dir object, curPath will be filled
 * 
 * @return int error type
 */
int getCurWorkDir() {
	if (getcwd(curPath, BUFFER_SIZE) == NULL)
		return ERROR_SYSTEM;
	else return RESULT_NORMAL;
}

/**
 * @brief split the command buffer into pieces by " ", 
 * like "ls -a | grep sh" -> ["ls","-a","|","grep","sh"]
 * 
 * @param command command buffer
 * @return int num of cmd pieces
 * commands[i] end with '\0'
 */
int splitCommands(char command[BUFFER_SIZE]) { 
	int num = 0;
	int i, j;
	for (i=0, j=0; i<strlen(command); i++) {
		if (command[i] != ' ') {
			commands[num][j++] = command[i];
		} else {
			if (j != 0) {
				commands[num][j] = '\0';
				++num;
				j = 0;
			}
		}
	}
	if (j != 0) {
		commands[num][j] = '\0';
		++num;
	}
	return num;
}

/**
 * @brief send terminal signal and exit the process
 * 
 * @return int error type
 */
int conduct_Exit() { 
	pid_t pid = getpid();
	if (kill(pid, SIGTERM) == -1) 
		return ERROR_EXIT;
	else return RESULT_NORMAL;
}

/**
 * @brief conduct CD function
 * 
 * @param commandNum the command's num in command buffer
 * @return int error type
 */
int conduct_Cd(int commandNum) { 
	int result = RESULT_NORMAL;

	if (commandNum < 2) {
		result = ERROR_MISS_PARAMETER;
	} else if (commandNum > 2) {
		result = ERROR_TOO_MANY_PARAMETER;
	} else {
		int ret = chdir(commands[1]);
		if (ret) result = ERROR_WRONG_PARAMETER;
	}
	return result;
}

/**
 * @brief help
 * 
 * @return int 
 */
int conduct_help(){
	printf("%s\n", commands[0]);
	system("cat help.txt");
}

/**
 * @brief conduct the external command
 * conduct logic: conduct cmd ---> have pipe(conduct pipe) ---> have redirect(conduct redirect)
 * @param commandNum 
 * @return int : error type 
 */
int conduct_cmd(int commandNum) { 
	pid_t pid = fork();
	if (pid == -1) {
		return ERROR_FORK;
	} else if (pid == 0) {
		/* deposit the STDIN and STDOUT, 
        since we will use redirect skill in function */
		int inFds = dup(STDIN_FILENO);
		int outFds = dup(STDOUT_FILENO);

		int result = conduct_pipe(0, commandNum);
		
		/*restore stdin and stdout*/
		dup2(inFds, STDIN_FILENO);
		dup2(outFds, STDOUT_FILENO);
		exit(result);
	} else {
		int status;
		waitpid(pid, &status, 0);
		return WEXITSTATUS(status);
	}
}

/**
 * @brief conduct the cmd that contains pipe '|'
 * 
 * @param left the first pieces
 * @param right 
 * @return int 
 */
int conduct_pipe(int left, int right) { 
	if (left >= right) return RESULT_NORMAL;
	/*check if contains '|'*/
	int idx = -1;
	for (int i=left; i<right; i++) {
		if (strcmp(commands[i], COMMAND_PIPE) == 0) {
			idx = i;
			break;
		}
	}
	if (idx == -1) { // without '|'
		return conduct_redirect(left, right);
	} else if (idx+1 == right) { // '|' without paraments, error
		return ERROR_PIPE_MISS_PARAMETER;
	}

	/* recursive part */
	int fd[2];
	if (pipe(fd) == -1) {
		return ERROR_PIPE;
	}
	int result = RESULT_NORMAL;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
	} else if (pid == 0) { // childProcess, conduct single cmd
		close(fd[0]);
		dup2(fd[1], STDOUT_FILENO); // STDOUT --> write port
		close(fd[1]);

		result = conduct_redirect(left, idx);
		exit(result);
	} else { // parentProcess, conduct recrusive part
		int status;
		waitpid(pid, &status, 0);
		int exitCode = WEXITSTATUS(status);
		
		if (exitCode != RESULT_NORMAL) { // childProcess error in exiting
			char info[4096] = {0};
			char line[BUFFER_SIZE];
			close(fd[1]);
			dup2(fd[0], STDIN_FILENO); // STDIN --> read port
			close(fd[0]);

			while(fgets(line, BUFFER_SIZE, stdin) != NULL)// read the error info from childProcess
				strcat(info, line);
			printf("%s", info); // print error info
			result = exitCode;
		} else if (idx+1 < right){
			close(fd[1]);
			dup2(fd[0], STDIN_FILENO); 
			close(fd[0]);

			result = conduct_pipe(idx+1, right); 
		}
	}
	return result;
}

/**
 * @brief conduct cmd with ">" or "<"
 * 
 * two symbol case:
 * case1: grep sh < output.txt > testout (good)
 * case2: cmd1 > cmd2 < cmd3 (X, not permit)
 * case3: cmd1 > cmd2 > cmd3 (X, not permit)
 * case4: cmd1 < cmd2 < cmd3 (X, not permit)
 * 
 * @param left 
 * @param right 
 * @return int 
 */
int conduct_redirect(int left, int right) { 
	if (!isCommandExist(commands[left])) {
		return ERROR_COMMAND;
	}	
	/* check if there are redirect actions */
	int inNum = 0, outNum = 0;
	char *inFile = NULL, *outFile = NULL;
	// the "right" of the cmd that we plan to conduct, 
	// for example, grep sh < output.txt > testout -----> grep sh (<)
	int idx = right; 

	for (int i=left; i<right; ++i) {
		if (strcmp(commands[i], COMMAND_IN) == 0) { // input redirect
			++inNum;
			if (i+1 < right)
				inFile = commands[i+1];
			else return ERROR_MISS_PARAMETER; // without file

			if (idx == right) idx = i;
		} else if (strcmp(commands[i], COMMAND_OUT) == 0) { // output redirect
			++outNum;
			if (i+1 < right)
				outFile = commands[i+1];
			else return ERROR_MISS_PARAMETER; 
				
			if (idx == right) idx = i;
		}
	}
	// printf("idx: %d\n", idx);
	// printf("right: %d\n", right);
	// printf("left: %d\n", left);
	// return 0;
	/* deal with the input redirect */
	if (inNum == 1) {
		FILE* fp = fopen(inFile, "r");
		if (fp == NULL) // input file not exits
			return ERROR_FILE_NOT_EXIST;
		fclose(fp);
	}
	/*ensure ">" and "<" equals 1*/
	if (inNum > 1) { 
		return ERROR_MANY_IN;
	} else if (outNum > 1) { 
		return ERROR_MANY_OUT;
	}

	int result = RESULT_NORMAL;
	pid_t pid = vfork();
	if (pid == -1) {
		result = ERROR_FORK;
	} else if (pid == 0) {
		/* redirect the stdin and stdout */
		if (inNum == 1)
			freopen(inFile, "r", stdin);
		if (outNum == 1)
			freopen(outFile, "w", stdout);
		/* conduct cmd */
		char* argvBuffer[BUFFER_SIZE];
		for (int i=left; i<idx; ++i)
			argvBuffer[i] = commands[i];
		argvBuffer[idx] = NULL;
		execvp(argvBuffer[left], argvBuffer+left);
		exit(errno); // conduct error
	} else {
		int status;
		waitpid(pid, &status, 0);
		int err = WEXITSTATUS(status); 
		if (err) {
			printf("\e[31;1mError: %s\n\e[0m", strerror(err));
		}
	}
	return result;
}

