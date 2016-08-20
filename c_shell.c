#include<unistd.h>//header file that is your code's entry point to various constant, type and function declarations that comprise the POSIX operating system API. 
#include<termios.h> //The termios.h header contains the definitions used by the terminal I/O interfaces
#include<signal.h>//The signal.h header defines a variable type sig_atomic_t, two function calls, and several macros to handle different signals reported during a program's execution.
#include<errno.h>//C Error Codes in Linux
#include<sys/types.h>
#include<fcntl.h>
#include<stdio.h>
#include <sys/prctl.h>//operations on a process
#include<stdlib.h>
#include<string.h>
#include<sys/wait.h>

char *start,*infile,*outfile;                        //stores the starting path from where shell is executed
char *args[100];			//the arguments list
char buf[BUFSIZ];
pid_t shell_pgid;			//shell's process group id
int interactive,shell_terminal,back,redirection;
pid_t child_pid;			//pid of the foreground running process (0 if shell is in foreground)

typedef struct bp{			//queue storing the details of the processes
	char pname[200];
	pid_t pid,pgid;
	struct bp *next;
}bp;

typedef struct linkd{
	char *argv[100];
	char *infile,*outfile;
	int argc;
	struct linkd *next;
}linkd;

linkd *commands;

bp *background_process;

/*Initialisation of the shell, performs a check if the shell is interactive or not.
  sets the global variables*/
void initialize_shell()
{
	shell_terminal=STDERR_FILENO;//for writing the errors. has a file descriptor of "2", with a unistd.h symbolic constant of STDERR_FILENO
	back=0;				
	infile=outfile=NULL;
	interactive=isatty(shell_terminal);/*The isatty() function tests whether fd is an open file descriptor referring to a terminal.
isatty() returns 1 if fd is an open file descriptor referring to a terminal; otherwise 0 is returned, and errno is set to indicate the error*/
	if(interactive)
	{
		while(tcgetpgrp(shell_terminal) != (shell_pgid=getpgrp()))
		{/*The function tcgetpgrp() returns the process group ID of the foreground process group on the terminal associated to fd, which must be the controlling terminal of the calling process.*/
			kill(- shell_pgid,SIGTTIN);//sigttin-Background process attempting read.
		}
	}
	signal (SIGINT, SIG_IGN);//Terminal interrupt signal.
	signal (SIGTSTP, SIG_IGN);//Terminal stop signal.
	signal (SIGQUIT, SIG_IGN);//Terminal quit signal.
	signal (SIGTTIN, SIG_IGN);//
	signal (SIGTTOU, SIG_IGN);//Background process attempting write.
	shell_pgid=getpid();
	if(setpgid(shell_pgid,shell_pgid)<0)
	{
		perror("Can't make shell a member of it's own process group");
		_exit(1);
	}
	tcsetpgrp(shell_terminal,shell_pgid);/*gives terminal access to the shell only.The function tcsetpgrp() makes the process group with process group ID pgrp the foreground process group on the terminal associated to fd, which must be the controlling terminal of the calling process, and still be associated with its session. When successful, tcsetpgrp() returns 0. Otherwise, it returns -1, and errno is set appropriately. */
}

void insert_command(int argc)
{
	int i;
	linkd *new=(linkd *)malloc(sizeof(linkd));//The C library function void *malloc(size_t size) allocates the requested memory and returns a pointer to it.
	for(i=0;i<argc;i++)
	{
		new->argv[i]=(char *)malloc(100);
		strcpy(new->argv[i],args[i]);
	}
	new->argv[i]=NULL;
	if(infile!=NULL)
	{
		new->infile=(char *)malloc(100);
		strcpy(new->infile,infile);
	}
	else
		new->infile=NULL;
	if(outfile!=NULL)
	{
		new->outfile=(char *)malloc(100);
		strcpy(new->outfile,outfile);
	}
	else
		new->outfile=NULL;		
	new->argc=argc;
	new->next=NULL;
	if(commands==NULL)
		commands=new;
	else
	{
		linkd *temp=commands;
		while(temp->next!=NULL)
			temp=temp->next;
		temp->next=new;
	}
}

/*insert a new process into the jobs table*/
void insert_process(char name[200],pid_t pid,pid_t pgid)
{
	bp *new=(bp *)malloc(sizeof(bp));
	strcpy(new->pname,name);
	new->pid=pid;
	new->pgid=pgid;
	new->next=NULL;
	if(background_process==NULL)
		background_process=new;
	else
	{
		bp *temp=background_process;
		while(temp->next!=NULL)
			temp=temp->next;
		temp->next=new;
	}
}

/*print the jobs in order of there starting time*/
void jobs()
{
	int i=1;
	bp *temp=background_process;
	while(temp!=NULL)
	{
		fprintf(stdout,"[%d]%s[%d]\n",i++,temp->pname,temp->pid);
		temp=temp->next;
	}
	return;
}

bp *getname(pid_t pid)
{
	bp *temp=background_process;
	while(temp!=NULL&&temp->pid!=pid)
		temp=temp->next;
	return temp;
}

pid_t get_pid(int n)
{
	n--;
	bp *temp=background_process;
	while(temp!=NULL&&n--)
		temp=temp->next;
	if(temp!=NULL)
		return temp->pid;
	else
		return -1;
}

/*remove a process from jobs table on termination*/
void remove_process(pid_t pid)
{
	if(background_process!=NULL)
	{
		bp *temp=background_process;
		if(background_process->pid==pid)
		{
			background_process=background_process->next;
			free(temp);
		}
		else
		{
			bp *r;
			while(temp!=NULL&&temp->pid!=pid)
			{
				r=temp;
				temp=temp->next;
			}
			if(temp!=NULL)
			{
				r->next=temp->next;
				free(temp);
			}
			else
				;
		}
	}
}

/*prints the prompt onto the stderr*/
void getprompt()
{
	char a[200],*user;
	user=getenv("LOGNAME");//get an environment variable
	gethostname(a,200);//to access or to change the host name of the current processor. On success, zero is returned. On error, -1 is returned, and errno is set appropriately. 
	char *c,fi[100];
	c=getcwd(NULL,0);
	if(strstr(c,start))// Returns a pointer to the first occurrence of str2 in str1, or a null pointer if str2 is not part of str1.
	{
		strcpy(fi,c+strlen(start));
		fprintf(stderr,"<%s@%s:~%s-BEE84-BCB38-CSE222>",user,a,fi);

	}
	else 
	{
		fprintf(stderr,"<%s@%s:%sBEE84-BCB38-CSE222>",user,a,c);
	}
	return;

}

/*handle the signals*/
void sig_handler(int signo)
{
	if(signo==SIGINT)
	{
		fprintf(stderr,"\n");
		getprompt();
	}
	else if(signo==SIGCHLD)
	{
		int status;
		pid_t pid;
		while((pid=waitpid(-1,&status,WNOHANG))>0)                    //return if no child has changed state
		{
			if(pid!=-1&&pid!=0)
			{
				if(WIFEXITED(status))				
				{
					bp* temp=getname(pid);
					if(temp!=NULL)
					{
						fprintf(stdout,"%s with pid %d exited normally\n",temp->pname,pid);
						remove_process(pid);
					}
				}
				else if(WIFSIGNALED(status))
				{
					bp* temp=getname(pid);
					if(temp!=NULL)
					{
						fprintf(stdout,"%s with pid %d signalled to exit\n",temp->pname,pid);
						remove_process(pid);
					}
				}
			}
		}
	}
}

int parser(char input[10000])
{
	int i1=1,ret;
	char *token,*str;
	char *saveptr1, *saveptr2;
	for(str=input,ret=0;;ret++,str=NULL)          //ret will count the no. of tokens separted by pipe
	{
		token=strtok_r(str,"|",&saveptr1);       //will split a string about delimiter
		if(token==NULL)
			break;
		char *temp=token;
		char *subtoken,*str1;
		int i;
		for(str1=temp,i=0;;str1=NULL,i++)
		{
			args[i]=strtok_r(str1," \t",&saveptr2);
			if(args[i]==NULL)							//a command is processed
				break;
			if(strcmp(args[i],"<")==0)					//redirection
			{
				if((infile=strtok_r(NULL," \t",&saveptr2))==NULL)
				{
					perror("No input file gievn!");
					return;
				}
				args[i--]=NULL;
			}               
			else if(strcmp(args[i],">")==0)
			{
				if((outfile=strtok_r(NULL," \t",&saveptr2))==NULL)
				{
					perror("No output file gievn!");
					return;
				}
				args[i--]=NULL;
			}
		}                                     //end of inner loop
		if(strcmp(args[i-1],"")==0)
		{
			args[i-1]=NULL;
			i--;
		}
		insert_command(i);						// i is the no. of tokens in a command
	}
	return ret;
}


/*the execution function, contains execution of binaries and user defined processes*/
int execute(int argc)
{
	int p=0;
	if(strcmp("quit",args[0])==0)		//quit shell
		_exit(0);
	
	else if(strcmp("jobs",args[0])==0)
	{
		jobs();											
		fflush(stdout);
	}
	else if(strcmp("overkill",args[0])==0)							//kill all processses
	{
		bp *temp;
		while(background_process!=NULL)
		{
			if(killpg(getpgid(background_process->pid),9)<0)
				perror("Error killing process");
		}
	}
	else 
	{
		pid_t pid;
		pid=fork();						//fork the child
		if(pid<0)
		{
			perror("Child process not created");
			p=-1;
		}
		else if(pid==0)		
		{
			/*inside child*/
			
			int run,in,out;
			prctl(PR_SET_PDEATHSIG, SIGHUP);		//kill the child when parent dies.(prevents formation of zombie process)
			setpgid(getpid(),getpid());			//put the child into it's own process group
			if(infile!=NULL)
			{
				in=open(infile,O_RDONLY);
				dup2(in,0);
				close(in);
			}
			if(outfile!=NULL)
			{
				out=open(outfile,O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
				dup2(out,1);
				close(out);
			}
			if(back==0)
				tcsetpgrp(shell_terminal,getpid());
			/*make the signals for the child as default*/
			signal (SIGINT, SIG_DFL);
			signal (SIGQUIT, SIG_DFL);
			signal (SIGTSTP, SIG_DFL);
			signal (SIGTTIN, SIG_DFL);
			signal (SIGTTOU, SIG_DFL);
			signal (SIGCHLD, SIG_DFL);
			run=execvp(args[0],args);
			if(run<0)
			{
				perror("Error exectuing command");
				_exit(-1);
			}
			_exit(0);
		}
		if(back==0)
		{	
			tcsetpgrp(shell_terminal,pid);				//to avoid racing conditions
			insert_process(args[0],pid,pid);
			int status;
			child_pid=pid;
			waitpid(pid,&status,WUNTRACED);				//wait for child till it terminates or stops
			if(!WIFSTOPPED(status))
				remove_process(pid);
			else
				fprintf(stderr,"\n[%d]+ stopped %s\n",child_pid,args[0]);
			tcsetpgrp(shell_terminal,shell_pgid);					//return control of terminal to the shell
		}
		else
			insert_process(args[0],pid,pid);

	}	
	fflush(stdout);
	return p;
}

int piped_execute(int pnum,char input[10000])
{
	int p=0,i,j=pnum-1,pgid,pipes[2*(pnum-1)],comc=0;			//declare pipes
	for(i=0;j--;i+=2)									// we have defined a pipe for each command
	{
		if((pipe(pipes+i))<0)						//open the pipes
		{	
			perror("pipe error");
			return -1;
		}
	}
	linkd *temp=commands;
	while(temp!=NULL)							
	{
		int pid=fork(),in,out;						//fork the child
		if(temp->next==NULL)
			insert_process(input,pid,pgid);
		if(comc==0&&pid!=0)
			pgid=pid;	                         // pgid is storing the proces id of child						
		if(pid!=0)			
			setpgid(pid,pgid);					//set the process group id
		if(pid==0)
		{
			/*set the signal of child to default*/
   			signal (SIGINT, SIG_DFL);
			signal (SIGQUIT, SIG_DFL);
			signal (SIGTSTP, SIG_DFL);
			signal (SIGTTIN, SIG_DFL);
			signal (SIGTTOU, SIG_DFL);
			signal (SIGCHLD, SIG_DFL);
			if(temp->outfile!=NULL)					//if there is a outfile
			{
				out=open(temp->outfile,O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
				dup2(out,1);
				close(out);
			}
			else if(temp->next!=NULL)				//if not the last command
			{
				if((dup2(pipes[2*comc+1],1))<0)
				{
					perror("dup2 error");
				}
			}
			if(temp->infile!=NULL)					//if there is an infile
			{
				in=open(temp->infile,O_RDONLY);
				dup2(in,0);
				close(in);
			}
			else if(comc!=0)					//if not the first command
			{
				if((dup2(pipes[2*(comc-1)],0))<0)
				{
					perror("dup2 error");
				}
			}
			for(i=0;i<2*(pnum-1);i++)				//close all pipes in the children
				close(pipes[i]);
			if((execvp(temp->argv[0],temp->argv))<0)
			{
				perror("Cannot execute");
				_exit(-1);
			}

		}
		else if(pid<0)							
		{
			perror("Could not fork child");
			return -1;
		}
		temp=temp->next;							//go to next command
		if(temp!=NULL)
			//	fprintf(stderr,"Up nxt%s\n",temp->argv[0]);
			comc++;
	}
	int status;									
	for(i=0;i<2*(pnum-1);i++)
	{
		close(pipes[i]);							//close all pipes in parent
	}
	if(back==0)
	{
		tcsetpgrp(shell_terminal,pgid);						//set the terminal control to child
		for(i=0;i<pnum;i++)
		{
			int ppp;
			ppp=waitpid(-pgid,&status,WUNTRACED);
			if(!WIFSTOPPED(status))
				remove_process(ppp);
			//else
			//	killpg(pgid,SIGSTOP);
		}
		tcsetpgrp(shell_terminal,shell_pgid);					//return control to parent
	}
	return p;
}

int main() 
{
	int p,c,argc,pnum;
	char input[10000],copy[10000];
	memset(buf,'\0',sizeof(buf));//void *memset(void *str, int c, size_t n) copies the character c (an unsigned char) to the first n characters of the string pointed to, by the argument str.
	int a=setvbuf(stdout,buf,_IOFBF,BUFSIZ);//int setvbuf(FILE *stream, char *buffer, int mode, size_t size) defines how a stream should be buffered.returns zero on success else, non-zero value is returned._IOFBF Values used to control the buffering of a stream in conjunction with the setvbuf function.fully controlled i/o
	if(a==0)
	{
		fprintf(stdout,"Welcome to Terminal-inception!\n");
		fflush(stdout);//fflush(stdout) to ensure that whatever you just written in a file/the console is indeed written out on disk/the console.
	}
	start=getcwd(NULL,0);
	initialize_shell();commands=NULL;
	while(1)
	{
		if(signal(SIGINT,sig_handler)==SIG_ERR)
			perror("Signal not caught!!");
		if(signal(SIGCHLD,sig_handler)==SIG_ERR)
			perror("signal not caught!!");
		getprompt();
		c=getc(stdin);
		while(isspace(c))
		{
			if(c=='\n')
				break;
			c=getc(stdin);
		}
		if(c=='\n')
		{
			fflush(stdout);// flushes the output buffer of a stream.
			continue;
		}
		ungetc(c,stdin);//int ungetc(int char, FILE *stream) pushes the character char (an unsigned char) onto the specified stream so that the this is available for the next read operation.
		fgets(input,10000,stdin);//char *fgets(char *str, int n, FILE *stream) reads a line from the specified stream and stores it into the string pointed to by str. It stops when either (n-1) characters are read, the newline character is read, or the end-of-file is reached, whichever comes first.
		while(isspace(input[strlen(input)-1]))		        //remove last spaces
			input[stderr,strlen(input)-1]='\0';
		if(input[strlen(input)-1]=='&')						//check if background
		{
			back=1;
			input[strlen(input)-1]='\0';
		}
		while(isspace(input[strlen(input)-1]))				//remove spaces in between command and &
			input[stderr,strlen(input)-1]='\0';
		strcpy(copy,input);
		pnum=parser(input);									//tells number of commands
		if(pnum==1)		
			p=execute(commands->argc);
		else
		{
			p=piped_execute(pnum,copy);
		}
		back=0;
		infile=outfile=NULL;
		commands=NULL;

	}
	return 0;
}
