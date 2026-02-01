/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#include "myshell.h"

/* Function prototypes */
void eval(char *cmdline);
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

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	argv[--argc] = NULL;

    return bg;
}
/* $end parseline */


