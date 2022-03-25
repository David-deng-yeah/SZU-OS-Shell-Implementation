#include<stdio.h>
#include<string.h>
#include<ctype.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdarg.h>

#include"anime.h"

#define BUFFER_SIZE 100// buffer size
#define MAX_CMD 10// max num of command
#define MAX_CMD_LEN 100// max len of each command

/*Global variables*/
int argc = 0;// num of the parameters
char* argv[MAX_CMD];// char* array of cmd, each char* pointer point the 2-D array command[][] 
char command[MAX_CMD][MAX_CMD_LEN];// store the content of the cmd
char buffer[BUFFER_SIZE];//act as a cache 
char bufferCopy[BUFFER_SIZE];// copy of cache

/*data process function*/
int get_input(char buffer[]);
void inputParse(char buffer[]);
/*conduct function*/
void conduct_cmd(int argc, char* argv[]);
int conduct_cd();
void conduct_pipe(char buffer[BUFFER_SIZE]);
void output_redirect(char buffer[BUFFER_SIZE]);

int main()
{
    while(1){
        printf("[Senjougahara]$ ");
        if(get_input(buffer) == 0)
            continue;
        inputParse(buffer);
        /*test case*/
        // printf("content: %s\n", bufferCopy);
        // printf("------------command----------------\n");
        // for(int i=0; i<MAX_CMD; i++){
        //     if(strlen(command[i]))
        //         printf("%s\n", command[i]);
        // }
        // printf("-------------argv-------------------\n");
        // for(int i=0; i<MAX_CMD; i++){
        //     if(argv[i] != NULL)
        //        printf("%s\n", argv[i]);
        // }

        /*now we have argc and argv, let's do sth interesting!*/
        conduct_cmd(argc, argv);
    }
    return 0;
}

int get_input(char buffer[]){
    memset(buffer, 0x00, BUFFER_SIZE);
    fgets(buffer, BUFFER_SIZE, stdin);// read from terminal
    buffer[strlen(buffer)-1] = 0x00;// remove the '/n' generate by fgets
    /*make a copy of argv, since some element of buffer may become '\0'*/
    strcpy(bufferCopy, buffer);
    return strlen(buffer);
}

void inputParse(char buffer[]){
    /*initialize argc, argv and command*/
    argc = 0;
    for(int i=0; i<MAX_CMD; i++) 
        argv[i] = NULL;
    memset(command, '\0', sizeof(command));
    int j = 0;

    /*initialize command*/
    for(int i=0; i<strlen(buffer); i++){
        if(buffer[i] != ' '){
            command[argc][j++] = buffer[i];
        }else{
            command[argc][j] = '\0';
            ++argc, j=0;
        }
    }
    if(j != 0) // make sure the end of each cmd is '\0'
        command[argc][j] = '\0';

    /*initialize argv*/
    argc = 0;
    int flag = 0;// trick for initalize argv
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (flag == 0 && !isspace(buffer[i])) {
            flag = 1;
            argv[argc++] = buffer + i;
        } else if (flag == 1 && isspace(buffer[i])) {
            flag = 0;
            buffer[i] = '\0';
        }
    }
    argv[argc] = NULL;// because of argc++
}

void conduct_cmd(int argc, char* argv[]){
    /*these cmd is internal command*/

    for(int i=0; i<MAX_CMD; i++){
        if(strcmp(command[i], ">") == 0){
            strcpy(buffer, bufferCopy);
            output_redirect(buffer);
            return;
        }
    }

    for(int i=0; i<MAX_CMD; i++){
        if(strcmp(command[i], "|") == 0){
            strcpy(buffer, bufferCopy);
            conduct_pipe(buffer);
            return;
        }
    }

    /*these cmd is external command*/
    if(strcmp(command[0], "help") == 0){
        printanime();
    }else if(strcmp(command[0], "exit") == 0){
        exit(0);
    }else if(strcmp(command[0], "cd") == 0){
        int res = conduct_cd();
        if(res == 0)printf("cd faild\n");
    }else{
        pid_t pid = fork();
        if(pid == -1){
            printf("error!\n");
        }else if(pid == 0){// child process
            execvp(argv[0], argv);
            //if this code running, that means the execvp get error
            printf("child process execvp run error!\n");
            exit(1);
        }else{// parent process
            //wait for the return of the child process
            int status;
            waitpid(pid, &status, 0);      
            int err = WEXITSTATUS(status); 
            if (err) { 
                printf("Error: %s\n", strerror(err));
            }  
        }
    }
    
}

int conduct_cd(){
    /*return 1 if success, else return 0*/
    if(argc != 2){
        return 0;
    }else{
        if(chdir(command[1]) !=0 ) return 0;
        else return 1;
    }
}

void conduct_pipe(char buffer[BUFFER_SIZE]){
    int idx = 0;
    for(; buffer[idx] != '\0'; idx++){
        if(buffer[idx] == '|') break;
    }
    char outputBuffer[idx];
    memset(outputBuffer, 0x00, idx);
    // char inputBuffer[strlen(buffer)-idx-1];
    char inputBuffer[strlen(buffer)-idx];
    memset(inputBuffer, 0x00, strlen(buffer)-idx);
    /*split the cmd into two cmd by '|'*/
    for(int i=0; i<idx-1; i++)
        outputBuffer[i] = buffer[i];
    for(int i=0; i<strlen(buffer)-idx-1; i++)
        inputBuffer[i] = buffer[idx+2+i];

    /*open a pipe array*/
    int pd[2];
    if(pipe(pd) < 0){
        perror("pipe error!\n");
        exit(1);
    }
    /*child process conduct output, parent process conduct input*/
    pid_t pid = fork();

    if(pid == -1){
        perror("error!\n");
        exit(1);
    }else if(pid == 0){// child process
        close(pd[0]);// close the read port
        inputParse(outputBuffer);
        dup2(pd[1], STDOUT_FILENO);// use pd[1](write port) as std_output
        execvp(argv[0], argv);
        if(pd[1] != STDOUT_FILENO)
            close(pd[1]);
    }else{// parent process
        /*parent process have to wait child process, since parent need child's output*/
        int status;
        waitpid(pid, &status, 0);      
        int err = WEXITSTATUS(status); 
        if (err) { 
            printf("Error: %s\n", strerror(err));
        }  

        close(pd[1]);// close write port
        inputParse(inputBuffer);
        dup2(pd[0], STDIN_FILENO);// use pd[0](read port) as std_input
        execvp(argv[0], argv);
        if(pd[0] != STDIN_FILENO)
            close(pd[0]);
    }
}

void output_redirect(char buffer[BUFFER_SIZE]){
    /*outfile*/
    char outputFile[BUFFER_SIZE];
    memset(outputFile, 0x00, BUFFER_SIZE);
    /*check the format of the cmd*/
    int num_redirect = 0;
    for(int i=0; i+1 < strlen(buffer); i++){
        if(buffer[i] == '>' && buffer[i+1] == ' ')
            num_redirect++;
    }
    if(num_redirect != 1){
        perror("redirect format error\n");
        return;
    }

    for(int i=0; i<argc; i++){
        if(strcmp(command[i], ">") == 0){
            if(i+1 < argc){
                strcpy(outputFile, command[i+1]);
            }else{
                perror("without outputfile!\n");
                return;
            }
        }
    }
    /*split the cmd by ">*/
    int idx = 0;
    for(; idx < strlen(buffer); idx++){
        if(buffer[idx] == '>') break;
    }
    buffer[idx-1] = '\0', buffer[idx] = '\0';
    inputParse(buffer);
    // printf("-------------argv-------------------\n");
    // for(int i=0; i<MAX_CMD; i++){
    //     if(argv[i] != NULL)
    //        printf("%s\n", argv[i]);
    // }
    // printf("%s\n", outputFile);
    pid_t pid = fork();
    if(pid == -1){
        perror("error!\n");
        exit(1);
    }else if(pid == 0){// child process
        /*open file*/
        int fd = open(outputFile, O_WRONLY|O_CREAT|O_TRUNC, 777);
        if(fd < 0) exit(1);
        /*conduct cmd*/
        dup2(fd, STDOUT_FILENO);// use fd as std_output
        execvp(argv[0], argv);
        if(fd != STDOUT_FILENO)
            close(fd);
        //if this code running, that means the execvp get error
        printf("child process execvp run error!\n");
        exit(1);
    }else{// parent process
        /*parent process have to wait child process, since parent need child's output*/
        int status;
        waitpid(pid, &status, 0);      
        int err = WEXITSTATUS(status); 
        if (err) { 
            printf("Error: %s\n", strerror(err));
        }  
    }
}