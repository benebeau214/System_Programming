/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#include "myshell.h"

/* Function prototypes */
void eval(char *cmdline);
void eval_pipe(char *cmdline); // phase2 용
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 



int main() 
{
    char cmdline[MAXLINE]; /* Command line */

    while (1) {
	/* Read */
	printf("CSE4100-SP-P2> "); //쉘을 실행하면 화면에 이와 같이 표시                   
	fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin))
	    exit(0);

	/* Evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    if(strchr(cmdline, '|') != NULL){
        eval_pipe(cmdline);
        return;
    }
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */

    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        if(pid = fork() == 0){ //자식 프로세스면
            if (execvp(argv[0], argv) < 0) {	// /bin/을 안 붙여도 입력을 정상적으로 처리할 수 있게 execvp 함수 사용
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
        

	/* Parent waits for foreground job to terminate */
	if (!bg){ 
	    int status;
        //waitpid를 통해 child의 종료를 기다림
        waitpid(pid, &status, 0);
	}
	else//when there is backgrount process!
	    printf("%d %s", pid, cmdline);
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit")) /* quit command */
    exit(0); 
    if (!strcmp(argv[0], "exit")) /* exit command */
    exit(0); 
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	return 1;

    //cd : change directory : 현재 있는 디렉터리 위치를 바꾼다.
    if(!strcmp(argv[0], "cd")){
        if(argv[1] == NULL){
            fprintf(stderr, "cd: missing argument\n");
        }else {
            if (chdir(argv[1]) != 0) {
                perror("cd");
            }
            
        }
        return 1;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */

//phase2용 함수

void eval_pipe(char *cmdline) 
{
    char *cmds[16];
    int num_cmds = 0;
    char *token = strtok(cmdline, "|");

    while (token != NULL && num_cmds < 16) {
        while (*token == ' ') token++; // trim left space
        cmds[num_cmds++] = token;
        token = strtok(NULL, "|");
    }

    int pipefd[2], prev_fd = -1;
    pid_t pid;

    for (int i = 0; i < num_cmds; i++) {
        char *argv[MAXARGS];
        parseline(cmds[i], argv);

        pipe(pipefd);
        if ((pid = fork()) == 0) {
            if (i != 0) { // not first command
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (i != num_cmds - 1) { // not last command
                close(pipefd[0]); // close read
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            } else {
                close(pipefd[0]);
                close(pipefd[1]);
            }

            if (execvp(argv[0], argv) < 0) {
                printf("%s: Command not found.\n", argv[0]);
                exit(1);
            }
        }

        if (prev_fd != -1) close(prev_fd);
        close(pipefd[1]);
        prev_fd = pipefd[0];
    }

    for (int i = 0; i < num_cmds; i++) {
        int status;
        wait(&status);
    }
}


/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;
    int argc;
    int bg;
    int pipenum;

    buf[strlen(buf) - 1] = ' ';  // 줄 끝 \n → 공백으로 변경
    while (*buf && (*buf == ' ')) buf++;  // 앞쪽 공백 제거

    char temp[MAXLINE];
    memset(temp, 0, sizeof temp);
    int cnt = 0;

    // 1. 문자열 가공: 따옴표 제거, | 주변 공백 삽입
    for (int i = 0; i < strlen(buf); i++) {
        if (buf[i] == '|') {
            pipenum++;
            temp[cnt++] = ' ';
            temp[cnt++] = '|';
            temp[cnt++] = ' ';
        } else if (buf[i] == '\'' || buf[i] == '\"') {
            char quote = buf[i++];
            while (buf[i] && buf[i] != quote) {
                temp[cnt++] = buf[i++];
            }
        } else {
            temp[cnt++] = buf[i];
        }
    }
    temp[cnt] = '\0';

    // 2. 가공된 문자열에서 argv 배열 구성
    argc = 0;
    char *ptr = temp;
    while ((delim = strchr(ptr, ' '))) {
        argv[argc++] = ptr;
        *delim = '\0';
        ptr = delim + 1;
        while (*ptr && (*ptr == ' ')) ptr++;  // 연속 공백 무시
    }
    argv[argc] = NULL;

    if (argc == 0) 
    return 1; 

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
    argv[--argc] = NULL;
    
    return bg;
}
/* $end parseline */


