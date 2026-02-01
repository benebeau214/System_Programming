#ifndef __MYSHELL_H__
#define __MYSHELL_H__

#define MAXARGS 128
#define MAXLINE 1024

/* Shell core functions */
void eval(char *cmdline);             
int parseline(char *buf, char **argv);
int builtin_command(char **argv);

/* Phase 2 : Pipe 처리 */
void eval_pipe(char *cmdline);        

#endif /* __MYSHELL_H__ */
