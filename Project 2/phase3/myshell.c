#include "csapp.h"
#include <errno.h>
#include "myshell.h"
#include <termios.h>
#include <unistd.h>

job_t jobs[MAXJOBS];
int next_job_id = 1;
pid_t fg_pid = 0;


/* Function prototypes */
void eval(char *cmdline);
void eval_pipe(char *cmdline, int bg); // phase2 용->phase3에 맞춰 수정
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

//phase 3
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void add_job(pid_t pid, char *cmdline, int running);
void delete_job(pid_t pid);
void list_jobs();
job_t *get_job_by_id(int job_id);

int main()
{
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, SIG_IGN);  // [Phase 3] 쉘은 Ctrl+Z 무시
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTTOU, SIG_IGN);  // [Phase 3] 터미널 제어권 변경 중 중단 방지
    setpgid(0, 0);             // [Phase 3] 쉘을 자신의 프로세스 그룹 리더로 설정
    tcsetpgrp(STDIN_FILENO, getpid());  // [Phase 3] 터미널 제어권을 쉘로 설정
    char cmdline[MAXLINE]; /* Command line */

    // SIGCHLD, SIGINT, SIGTSTP 핸들러 처리리
    Signal(SIGCHLD, sigchld_handler);
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP, sigtstp_handler);

    while (1) {
    /* Read */
    printf("CSE4100-SP-P2> "); //쉘을 실행하면 화면에 이와 같이 표시 
    fflush(stdout);
    if (fgets(cmdline, MAXLINE, stdin) == NULL) {
    if (feof(stdin)) exit(0);
        continue;
    }
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
    if (argv[0] == NULL) return;

    if (strchr(cmdline, '|') != NULL) {
        char cmd_copy[MAXLINE];
        strcpy(cmd_copy, cmdline);
        eval_pipe(cmd_copy, bg);
        return; /* Ignore empty lines */
    }

    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        if ((pid = fork()) == 0) {
        signal(SIGTSTP, SIG_DFL); // [Phase 3] 자식은 Ctrl+Z에 반응하도록 복원 //자식 프로세스면
            setpgid(0, 0);
            if (execvp(argv[0], argv) < 0) {    // /bin/을 안 붙여도 입력을 정상적으로 처리할 수 있게 execvp 함수 사용
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }

        setpgid(pid, pid); // 자식 프로세스를 새로운 프로세스 그룹 리더로 지정
        /* Parent waits for foreground job to terminate */
        if (!bg) {
            fg_pid = pid; //현재 포그라운드 프로세스의 pid 저장
            tcsetpgrp(STDIN_FILENO, pid); //터미널 제어권을 자식 프로세스 그룹에 넘김
            int status;
            //waitpid를 통해 child의 종료를 기다림
            waitpid(pid, &status, WUNTRACED); //자식 종료/중지 후, 터미널 제어권을 다시 쉘로 회수
            tcsetpgrp(STDIN_FILENO, getpgrp());
            if (WIFSTOPPED(status)) {
                add_job(pid, cmdline, 0); //자식이 중지되었으면 jobs 리스트에 Stopped 상태로 등록
                printf("[%d] Stopped %s", next_job_id - 1, cmdline); //사용자에게 상태 출력
            } else {
                delete_job(pid); //자식이 정상 종료되었으면 job 리스트에서 제거
            }
            fg_pid = 0; //현재 포그라운드 프로세스 pid 초기화
        } else {//when there is background process!
            add_job(pid, cmdline, 1);
            printf("[%d] %d %s", next_job_id - 1, pid, cmdline);
        }
    }
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
    if (!strcmp(argv[0], "cd")) {
        if (argv[1] == NULL) {
            fprintf(stderr, "cd: missing argument\n");
        } else {
            if (chdir(argv[1]) != 0) perror("cd");
        }
        return 1;
    }

    if (!strcmp(argv[0], "jobs")) {
        list_jobs();
        return 1;
    }

    // 'fg' 명령 처리 : 특정 job을 포그라운드로 가져옴
    if (!strcmp(argv[0], "fg")) {
        if (argv[1][0] == '%') {
		int job_id = atoi(argv[1] + 1);
		job_t *job = get_job_by_id(job_id);
        if (job) {
            fg_pid = job->pid;
            tcsetpgrp(STDIN_FILENO, job->pid);
            kill(-job->pid, SIGCONT);
            int status;
            waitpid(job->pid, &status, WUNTRACED);
            tcsetpgrp(STDIN_FILENO, getpgrp());
            if (WIFSTOPPED(status)) {
                job->running = 0;
                printf("[%d] Stopped %s", job->job_id, job->cmdline);
            } else {
                delete_job(job->pid);
            }
            fg_pid = 0;
        } else {
		printf("No such job\n");
	   }
	}
        return 1;
    }

    // 'bg' 명령 처리 : 중지된 job을 백그라운드로 재시작 
    if (!strcmp(argv[0], "bg")) {
        if (argv[1][0] == '%') {
        int job_id = atoi(argv[1] + 1);
        job_t *job = get_job_by_id(job_id);
        if (job) {
            kill(-job->pid, SIGCONT);
            job->running = 1;
	    printf("[%d] Running %s", job->job_id, job->cmdline);
        }else{
		printf("No such job\n");
	     }
	}
        return 1;
    }

    //'kill' 명령 처리 : 지정된 job을 종료
    if (!strcmp(argv[0], "kill")) {
        if (argv[1][0] == '%') {
            
        int job_id = atoi(argv[1] + 1);
        job_t *job = get_job_by_id(job_id);
        if (job) {
            kill(job->pid, SIGKILL);
            delete_job(job->pid);
        }else{
		printf("No such job\n");
	     }	
	}
        return 1;
    }

    return 0;
}
/* $end eval */

//phase2용 함수(phase3에 맞춰 수정)

void eval_pipe(char *cmdline, int bg) {
    char *cmds[16];
    int num_cmds = 0;

    char original_cmd[MAXLINE];
    strcpy(original_cmd, cmdline);
    char *token = strtok(cmdline, "|");

    while (token != NULL && num_cmds < 16) {
        while (*token == ' ') token++; // trim left space
        cmds[num_cmds++] = token;
        token = strtok(NULL, "|");
    }

    int pipefd[2], prev_fd = -1;
    pid_t pid, last_pid;

    for (int i = 0; i < num_cmds; i++) {
        char *argv[MAXARGS];
        parseline(cmds[i], argv);
        pipe(pipefd);

        if ((pid = fork()) == 0) {
        signal(SIGTSTP, SIG_DFL); // 자식은 Ctrl+Z에 반응하도록 복원
            setpgid(0, 0);
            if (prev_fd != -1) {
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

        setpgid(pid, pid);
        if (prev_fd != -1) close(prev_fd);
        close(pipefd[1]);
        prev_fd = pipefd[0];

        if (i == num_cmds - 1) last_pid = pid;
    }

    if (bg) {
        add_job(last_pid, cmdline, 1);
        printf("[%d] %d %s\n", next_job_id - 1, last_pid, original_cmd);
    } else {
        fg_pid = last_pid;
        tcsetpgrp(STDIN_FILENO, last_pid);
        int status;
        waitpid(last_pid, &status, WUNTRACED);
        tcsetpgrp(STDIN_FILENO, getpid());
        if (WIFSTOPPED(status)) {
            add_job(last_pid, cmdline, 0);
            printf("[%d] Stopped %s", next_job_id - 1, original_cmd);
        } else {
            delete_job(last_pid);
        }
        fg_pid = 0;
    }
}


/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim; /* Points to first space delim*/
    int argc;    /* Number of args */
    int bg;      /* Background job? */
    int pipenum;

    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    char temp[MAXLINE];
    memset(temp,0,sizeof temp);
    int cnt = 0;

    // 1. 문자열 가공: 따옴표 제거, | 주변 공백 삽입
    for (int i = 0; i < strlen(buf); i++)
    {
        if (buf[i] == '|')
        {
            pipenum++;
            temp[cnt++] = ' ';
            temp[cnt++] = '|';
            temp[cnt++] = ' ';
        }
        else if (buf[i-1]!=' ' && buf[i]=='&')
        {
            temp[cnt++] = ' ';
            temp[cnt++] = '&';
        }
        else if (buf[i] == '\'' || buf[i] == '\"') 
        {
            char standard = buf[i];
            while (buf[++i] != standard)
            {
                temp[cnt++] = buf[i];
            }
        }
        else
        {
            temp[cnt++] = buf[i];
        }
    }
    strcpy(buf,"");
    strcat(buf,temp);

    // 2. 가공된 문자열에서 argv 배열 구성
    argc = 0;
    while ((delim = strchr(buf, ' ')))
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore*/
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */ 
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;


    return bg;
}
/* $end parseline */

/*phase 3용 */

//자식 프로세스 종료 시 jobs 배열에서 제거
void sigchld_handler(int sig) {
    int olderrno = errno;
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            delete_job(pid);
        }
    }
    errno = olderrno;
}

//포그라운드 프로세스에 SIGIN 전달 (Ctrl+C)
void sigint_handler(int sig) {
    if (fg_pid > 0) kill(-fg_pid, SIGINT);
}

//포그라운드 job을 중지시키고 jobs에 등록 (Ctrl+Z)
void sigtstp_handler(int sig) {
    if (fg_pid > 0) kill(-fg_pid, SIGTSTP);
}

// 백그라운드 또는 중지된 job을 jobs 배열에 등록하는 함수
void add_job(pid_t pid, char *cmdline, int running) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].job_id = next_job_id++;
            jobs[i].pid = pid;
            jobs[i].running = running;
            strncpy(jobs[i].cmdline, cmdline, MAXLINE);
            return;
        }
    }
}

//종료된 job을 jobs 배열에서 제거하는 함수
void delete_job(pid_t pid) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].pid = 0;
            return;
        }
    }
}

//job_id를 기준으로 jobs 배열에서 job을 찾아 반환하는 함수
job_t *get_job_by_id(int job_id) {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].job_id == job_id) return &jobs[i];
    }
    return NULL;
}

//현재 등록된 모든 job 정보를 출력하는 함수
void list_jobs() {
    for (int i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] %d %s %s",
                   jobs[i].job_id,
                   jobs[i].pid,
                   jobs[i].running ? "Running" : "Stopped",
                   jobs[i].cmdline);
        }
    }
}
