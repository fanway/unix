//#define _SVID_SOURCE
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <dirent.h>
#include <grp.h>
#include <time.h>
#include <pwd.h>
#define PROC_FS "/proc"

struct dirent *entry;

int shell_active = 1; // требуется для команды exit

//  команды оформлены в виде макросов
//  доп. информация http://en.wikipedia.org/wiki/C_preprocessor

#define SHCMD(x) int shcmd_##x (char* cmd, char* params[])
#define SHCMD_EXEC(x) shcmd_##x (params[0], params)
#define IS_CMD(x) strncmp(#x,cmd,strlen(cmd)) == 0

// Встроенные команды
// - pwd, exit
struct ls_struct
{
    int l;
    int a;
    int i;
};
struct wc_struct
{
    int L;
    int l;
    int w;
    int c;
};
#define LIST_LENGTH 4096
int count_line_words(char* line, size_t length, struct wc_struct* flags)
{
    char delim[] = " \t\n";
    char* word = strtok(line, delim);
    int word_count = 0;
    if (word == NULL)
        return word_count;

    word_count++;
    while ((word = strtok(NULL, delim)) != NULL)
        word_count++;
    return word_count;
}

int shcmd_wc(char* cmd, char* params[])
{
    int params_count = 0;
    while (params[params_count] != NULL)
        params_count++;
    struct wc_struct flags;
    flags.c = flags.l = flags.w = 0;
	//flags.L = 0;
    char opts[] = "cwlL";
    int opt;
    while ((opt = getopt(params_count, params, opts)) != -1)
    {
        switch(opt)
        {
        case 'c':
            flags.c = 1;
            break;
        case 'w':у
            flags.w = 1;
            break;
        case 'l':
            flags.l = 1;
            break;
			/*
        case 'L':
            flags.L = 1;
            break;*/
        }
    }
    int i;
    printf("optind == %d\n", optind);
    for (i = 0; i < params_count; i++)
        printf("%d: %s\n", i, params[i]);
		/*
    if (flags.c == flags.w && flags.w == flags.l && flags.l == flags.L && flags.L == 0)
    {
        flags.c = flags.w = flags.l = 1;
    }*/
	if (flags.c == flags.w && flags.w == flags.l && flags.l == 0)
    {
        flags.c = flags.w = flags.l = 1;
	}
    char* line = NULL;
    size_t n;
    //size_t total_longest_line = 0;
    size_t total_byte_count = 0;
    size_t total_line_count = 0;
    size_t total_word_count = 0;

    size_t file_byte_count = 0;
    for (i = optind; i < params_count; i++)
    {
        //size_t file_longest_line = 0;
        file_byte_count = 0;
        size_t file_line_count = 0;
        size_t file_word_count = 0;
        FILE* file = fopen(params[i], "r");
        if (file == NULL)
        {
            perror("fopen");
            return -1;
        }
        ssize_t read_bytes;
        while ((read_bytes  = getline(&line, &n, file)) != -1)
        {
            file_byte_count += read_bytes;
            file_line_count++;
			/*
            if (read_bytes > file_longest_line)
            {
                file_longest_line = read_bytes;
                if (line[read_bytes-1] == '\n')
                    file_longest_line--;
            }
			*/
            if (n <= 1)
                continue;
            file_word_count += count_line_words(line, n, &flags);
        }
        total_byte_count += file_byte_count;
        total_line_count += file_line_count;
        total_word_count += file_word_count;
		/*
        if (total_longest_line < file_longest_line)
        {
            total_longest_line = file_longest_line;
        }
		*/
        if (flags.l)
            printf("%6d", file_line_count);
        if (flags.w)
            printf("%6d", file_word_count);
        if (flags.c)
            printf("%6d", file_byte_count);
			/*
        if (flags.L)
            printf("%6d", file_longest_line);
			*/
        printf("  %s\n", params[i]);
    }
    if (total_byte_count != file_byte_count)
    {
        if (flags.l)
            printf("%6d", total_line_count);
        if (flags.w)
            printf("%6d", total_word_count);
        if (flags.c)
            printf("%6d", total_byte_count);
			/*
        if (flags.L)
            printf("%6d", total_longest_line);
			*/
        printf("  total\n");
    }

    if (line != NULL)
        free(line);
    return 0;
}

int always_true(const struct dirent *d)
{
    return 1;
}

int print_ls_l(const struct dirent* d, const char* path, int print_inode)
{
    char time[256];
    struct stat st;
    struct passwd *pwd;
    struct group *grp;
    int mode,right_ind;
    char* rights = "rwxrwxrwx";
    char *nuser,*ngroup;
    char fullname[1024];
    strcpy(fullname, path);
    if (fullname[strlen(fullname)] != '/')
        strcat(fullname, "/");
    strcat(fullname, d->d_name);
    lstat(fullname,&st);
	mode = st.st_mode;
	if (print_inode)
    {
        printf("%d ", d->d_ino);
    }
	S_ISDIR(mode)?printf("d"):printf("-");
	mode &= 0777;
	right_ind = 0;
	while( right_ind < 9 )
	{
	    mode&256?printf("%c",rights[right_ind]):printf("-");
	    mode <<= 1;
	    right_ind++;
	}
	if (pwd=getpwuid(st.st_uid))
        nuser=pwd->pw_name;
    else
        sprintf(nuser,"%d",st.st_uid);

	if (grp=getgrgid(st.st_gid))
        ngroup=grp->gr_name;
    else
        sprintf(ngroup,"%d",st.st_gid);

	strftime(time, sizeof(time), "%Y-%m-%d %H:%M", localtime(&st.st_mtime));

    if (print_inode != 0)
        printf("%3d %6s %6s %5d %s %s\n",st.st_nlink,nuser,ngroup,st.st_size, time, d->d_name);
    else
        printf("%3d %6s %6s %5d %s %s\n",st.st_nlink,nuser,ngroup,st.st_size, time, d->d_name);

    return 0;
}
int shcmd_ls(char* cmd, char* params[])
{
    int params_count = 0;
    while (params[params_count] != NULL)
        params_count++;
    printf("ls started\n");
    struct ls_struct flags;
    flags.l = flags.a = flags.i = 0;
    char opts[] = "lai";
    int opt;
    while ((opt = getopt(params_count, params, opts)) != -1)
    {
        switch(opt)
        {
        case 'l':
            flags.l = 1;
            break;
        case 'a':
            flags.a = 1;
            break;
        case 'i':
            flags.i = 1;
            break;
        }
    }
    char path[500];
    if (params[optind] == NULL)
        strcpy(path, getenv("PWD"));
    else
        strcpy(path, params[optind]);
    printf("optind = %d\n", optind);
    printf("path = %s\n", path);
    int i;
    for (i = 0; i < params_count; i++)
        printf("params[%d] = %s\n", i, params[i]);
    struct dirent ** entry_list;
    int entry_count = scandir(path, &entry_list, always_true, alphasort);
    int current_length = 0;
    int terminal_width = 80;
    for (i = 0; i < entry_count; i++)
    {
        if (flags.a == 0 && entry_list[i]->d_name[0] == '.')
            continue;
        if (flags.l)
            print_ls_l(entry_list[i], path, flags.i);
        else
        {
            char filename[1024];
            if (flags.i == 0)
                strcpy(filename, "");
            else
                sprintf(filename, "%d ", entry_list[i]->d_ino);
            strcat(filename, entry_list[i]->d_name);
            if (current_length + strlen(filename) + 1 >= terminal_width && current_length > 0)
            {
                printf("\n");
                current_length = 0;
            }
            printf("%s  ", filename);
            current_length += strlen(filename) + 2;
        }
    }

    printf("\n");
    return 0;
}

typedef struct proc_t
{
	int pid;
	char cmd[256];
	unsigned char state;
	int ppid;
	int pgrp;
	int session;
	int tty_nr;
	int tpgid;
	uint flags;
	uint minflt;
	uint cminflt;
	uint majflt;
	uint cmajflt;
	int utime;
	int stime;
	int cutime;
	int cstime;
	int priority;
	uint nice;
	int zero;
	uint itrealvalue;
	int starttime;
	uint vsize;
	uint rss;
	uint rlim;
	uint startcode;
	uint endcode;
	uint startstack;
	uint kstkesp;
	uint kstkeip;
	int signal;
	int blocked;
	int sigignore;
	int sigcatch;
	uint wchan;
//additional
	int uid;
	time_t ctime;
} proc_t;
void read_proc_stat(struct proc_t *ps, char* fname)
{
	char path[256];

	struct stat st;

	sprintf(path,"/%s/%s/stat", PROC_FS, fname);

	FILE *file = fopen(path,"r");
	fscanf(file,"%d %s %c %d %d %d %d %d %u %u %u %u %u %d %d %d %d %d %u %d %u %d %u %u %u %u %u %u %u %u %d %d %d %d %u",
		&ps->pid, ps->cmd, &ps->state, &ps->ppid, &ps->pgrp, &ps->session,
		&ps->tty_nr, &ps->tpgid, &ps->flags, &ps->minflt, &ps->cminflt,
		&ps->majflt, &ps->cmajflt, &ps->utime, &ps->stime, &ps->cutime,
		&ps->cstime, &ps->priority, &ps->nice, &ps->zero, &ps->itrealvalue,
		&ps->starttime, &ps->vsize, &ps->rss, &ps->rlim, &ps->startcode,
		&ps->endcode, &ps->startstack, &ps->kstkesp, &ps->kstkeip, &ps->signal,
		&ps->blocked, &ps->sigignore, &ps->sigcatch, &ps->wchan
	);

	sprintf(path,"/%s/%s", PROC_FS, fname);
	lstat(path,&st);

	ps->uid = st.st_uid;
	ps->ctime = st.st_ctime;
	fclose(file);
}

int shcmd_ps(char* cmd, char* params[])
{
	struct proc_t ps;

	DIR *Dir = opendir(PROC_FS);

	char time[256];

	//printf("UID\t  PID  PPID  NI\tSTIME\tTTY\tTIME\tCMD\n");
	printf("UID\t  PID  PPID  NI\tSTIME\tCMD\n");
	while ((entry = readdir(Dir))) {
		if(!isdigit(*entry->d_name))
			continue;
		read_proc_stat(&ps,entry->d_name);
		printf("%s\t",getpwuid(ps.uid)->pw_name);
		printf("%5d ",ps.pid);
		printf("%5d ",ps.ppid);
		printf("%2d\t",ps.nice);

		strftime(time, sizeof(time), "%H:%M", localtime(&ps.ctime));
		printf("%s\t",time);
		//printf("\t");
		//printf("\t");
		printf("%s\t",ps.cmd);
		printf("\n");
	}

	closedir(Dir);
}
int shcmd_cd(char* cmd, char* params[]){
    if (params[1] == NULL) {
    fprintf(stderr, "exec: expected argument to \"cd\"\n");
  } else {
    if (chdir(params[1]) != 0) {
      perror("exec");
    }
    else
        {
            char cwdd[50];
            getcwd(cwdd, 50);
            setenv("PWD", cwdd, 1);
        }
  }
  return 1;
}
SHCMD(pwd)
{
	printf("%s\n",getenv("PWD"));
	return 0;
}

SHCMD(exit)
{
	shell_active = 0;
	printf("Bye, bye!\n");
	return 0;
}


// выполнение команды с параметрами
void my_exec(char *cmd)
{
	char *params[256]; //параметры команды разделенные пробелами
	char *token;
	int np;

	token = strtok(cmd, " ");
	np = 0;
	while( token && np < 255 )
	{
		params[np++] = token;
		token = strtok(NULL, " ");
	}
	params[np] = NULL;

	// выполнение встроенных команд
	if( IS_CMD(pwd) )
		SHCMD_EXEC(pwd);
	else
	if( IS_CMD(exit) )
		SHCMD_EXEC(exit);
	else
	if (IS_CMD(cd))
		SHCMD_EXEC(cd);
	else
	if (IS_CMD(ps))
	    SHCMD_EXEC(ps);
	else
	if (IS_CMD(ls))
		SHCMD_EXEC(ls);
	else
	if (IS_CMD(wc))
		SHCMD_EXEC(wc);
	else
	{ // иначе вызов внешней команды
		execvp(params[0], params);
		perror("exec"); // если возникла ошибка при запуске
	}
}
// рекурсивная функция обработки конвейера
// параметры: строка команды, количество команд в конвейере, текущая (начинается с 0 )
int exec_conv(char *cmds[], int n, int curr)
{
	int fd[2],i;
	if( pipe(fd) < 0 )
	{
		printf("Cannot create pipe\n");
		return 1;
	}

	if( n > 1 && curr < n - 2 )
	{ // first n-2 cmds
		if( fork() == 0 )
		{
			dup2(fd[1], 1);
			close(fd[0]);
			close(fd[1]);
			my_exec(cmds[curr]);
			exit(0);
		}
		if( fork() == 0 )
		{
			dup2(fd[0], 0);
			close(fd[0]);
			close(fd[1]);
			exec_conv(cmds,n,++curr);
			exit(0);
		}
	}
	else
	{ // 2 last cmds or if only 1 cmd
		if( (n == 1) && (strcmp(cmds[0],"exit") == 0 || strstr(cmds[0], "cd") != NULL))
		{ // для случая команды exit обходимся без fork, иначе не сможем завершиться
			close(fd[0]);
			close(fd[1]);
			my_exec(cmds[curr]);
			return 0;
		}
		if( fork() == 0 )
		{
			if( n > 1 ) // if more then 1 cmd
				dup2(fd[1], 1);
			close(fd[0]);
			close(fd[1]);
			my_exec(cmds[curr]);
			exit(0);
		}
		if( n > 1 && fork()==0 )
		{
			dup2(fd[0], 0);
			close(fd[0]);
			close(fd[1]);
			my_exec(cmds[curr+1]);
			exit(0);
		}
	}
	close(fd[0]);
	close(fd[1]);

	for( i = 0 ; i < n ; i ++ ) // ожидание завершения всех дочерних процессов
		wait(0);

	return 0;
}
// главная функция, цикл ввода строк (разбивка конвейера, запуск команды)
int main()
{
	char cmdline[1024];
	char *p, *cmds[256], *token;
	int cmd_cnt;

	while( shell_active )
	{
		printf("[%s]# ",getenv("PWD"));
		fflush(stdout);
		fgets(cmdline,1024,stdin);
		if( (p = strstr(cmdline,"\n")) != NULL ) p[0] = 0;

		token = strtok(cmdline, "|");
		for(cmd_cnt = 0; token && cmd_cnt < 256; cmd_cnt++ )
		{
			cmds[cmd_cnt] = strdup(token);
			token = strtok(NULL, "|");
		}
		cmds[cmd_cnt] = NULL;

		if( cmd_cnt > 0 )
		{
			exec_conv(cmds,cmd_cnt,0);
		}
	}

	return 0;
}