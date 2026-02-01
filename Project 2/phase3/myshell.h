#ifndef __MYSHELL_H__
#define __MYSHELL_H__

#define MAXARGS 128
#define MAXJOBS 16

//#ifndef WCONTINUED
#define WCONTINUED 0

#include <sys/types.h>

/* Shell core functions */
void eval(char *cmdline);             
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

/* Phase 2 : Pipe 처리*/
void eval_pipe(char *cmdline, int bg);  

/* phase 3 : Job control */

//job_t 구조체
typedef struct {
    int job_id;
    pid_t pid;
    char cmdline[MAXLINE];
    int running;  // 1 = running, 0 = stopped
} job_t;

extern job_t jobs[MAXJOBS]; //백그라운드 및 중지된 job들을 저장하는 전역 배열
extern int next_job_id; //새로운 job_id 생성을 위한 전역 변수

void add_job(pid_t pid, char *cmdline, int running); //jobs 배열에 새로운 job을 추가하는 함수
void delete_job(pid_t pid); //특정 pid를 가진 job을 jobs 배열에서 제거하는 함수
job_t *get_job_by_id(int job_id); //job_id로 job 정보를 검색하는 함수 (없으면 NULL 반환)
void list_jobs(void); //현재 jobs 배열에 등록된 모든 job을 출력하는 함수

/* Signal handlers */
void sigchld_handler(int sig); // SIGCHLD 처리: 자식 프로세스 종료 시 job 제거
void sigint_handler(int sig); //SIGINT 처리: 포그라운드 프로세스에 Ctrl+C 전달
void sigtstp_handler(int sig); //SIGTSTP 처리: 포그라운드 프로세스에 Ctrl+Z 전달 및 job 중지 등록


#endif /* __MYSHELL_H__ */
