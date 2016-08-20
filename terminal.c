#include<unistd.h>//header file that is your code's entry point to various constant, type and function declarations that comprise the POSIX operating system API. 
#include<termios.h> //The termios.h header contains the definitions used by the terminal I/O interfaces
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
	
	
	shell_pgid=getpid();
	
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
		fprintf(stderr,"<%s@%s:~%s-CSE220-BEE84-BCB38>",user,a,fi);

	}
	else 
	{
		fprintf(stderr,"<%s@%s:%s-CSE220-BEE84-BCB38>",user,a,c);
	}
	return;

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
	if(strcmp("DEADPOOL",args[0])==0)		//quit shell
		_exit(0);
	
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
			//inside child*/
			
			int run;
			
			run=execvp(args[0],args);
			if(run<0)
			{
				perror("Error exectuing command");
				_exit(-1);
			}
			_exit(0);
		}

	}	
	fflush(stdout);
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
		fprintf(stdout,"Welcome\n");
		fflush(stdout);//fflush(stdout) to ensure that whatever you just written in a file/the console is indeed written out on disk/the console.
	}
	start=getcwd(NULL,0);
	initialize_shell();commands=NULL;
	while(1)
	{
		
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
		p=execute(commands->argc);
		back=0;
		infile=outfile=NULL;
		commands=NULL;

	}
	return 0;
}
