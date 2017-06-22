#define _GNU_SOURCE//To implicit get_current_dir_name()//but makes the code non portable

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <fcntl.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

/*Function declartions*/
int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

/*Lookup table of built-in commands- includes doc on the comaands*/
fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_cd, "cd", "change the shell working directory"},
  {cmd_pwd, "pwd", "get current dir name"}
};

/*Shell functions*/

/* Print name of current/working directory */
int cmd_pwd(unused struct tokens *tokens){
	printf("%s\n",get_current_dir_name());//gets PWD variable path(why it can only work on GNU)
	/* Another way to get cwd
    char cwd[4096]; //set to max chars of line
    getcwd(cwd, 4096);//copies an absolute pathname
    printf("%s\n", cwd);//prints pathname
	*/
	return 1;
}

//#include <pwd.h> getpwuid(getuid())->pw_dir; //can do this to get home directory but MAN suggests aginst it
/* Change the shell working directory */
int cmd_cd(struct tokens *tokens) {
        /*When our CD command gets NULL or ~ as the first arg it goes to home directory*/
        if(tokens_get_token(tokens,1) == NULL || strcmp(tokens_get_token(tokens,1),"~")==0)
        {
           chdir(getenv("HOME"));/*Gets the path deifned as HOME var and chdir to it*/
        }
        else if(chdir(tokens_get_token(tokens,1)) != 0)/*Tries to change path to first arg if error prints error*/
        {
            perror("cd");
        }

    return 1;
}

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
//Added a check for a NULL
  if(cmd == NULL)//no word
    return -3;//we like the number 3
    //
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))//checks if word is the same word as the comand word
      return i;//retuns the index of the command in the cmd table
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}
/* Changes chars of changeMe accordinly*/
void changeCharAtoB(char a, char b, char *changeMe)
{
            for(unsigned int i=0;i<strlen(changeMe);i++)
            {
                if(changeMe[i]==a)//When finds the "unwanted" char changes to the "wanted" one
                {
                    changeMe[i]=b;
                }
            }
}

/* Counts how many char A are in inspectMe*/
int countCharA(char a, char *inspectMe)
{
unsigned int count=0;
            for(unsigned int i=0;i<strlen(inspectMe);i++)
            {
                if(inspectMe[i]==a)
                {
                    count++;
                }
            }
return count;
}

int main(unused int argc, unused char *argv[]) {
  init_shell();
  /* Atribue for printing lines of shell*/
  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if(fundex == -3){}
    else if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    }
    else {
      /* REPLACE this to run commands as programs. */

        pid_t pid ,unused wpid/*wpid is used but still get warning*/;//Init pid
        int status;//status to know when child is dead

        pid = fork();//fork so once command runs returns to shell
        if (pid == 0){//child
        if (countCharA('<',line)+countCharA('>',line)>1)//counting how many brackets are in line
        {
            fprintf(stdout,"Too many '<'/'>' Arguments\n");//our shell dosent suuport that
            exit(EXIT_FAILURE);
        }
        bool gotSymb=false;//, leftBracket=false,rightBracket=false;
        char *fileName;
        int newfd;
        unsigned int tokensLength = tokens_get_length(tokens);//
        char *arr[tokensLength+1];//+1 for NULL at the
        /*Puts words we got from line into array*/
        for (unsigned int i=0;i <tokensLength ;i++)
            {
                if(!gotSymb)
                {
                arr[i]=tokens_get_token(tokens,i);
               /* Checking '<' or '>' inside a word */
                    for (unsigned int j=0;j<strlen(arr[i]);j++)
                    {

                        if(arr[i][j]=='>'||arr[i][j]=='<')
                        {
                            gotSymb=true;//our shell dosent suuport more than one bracket so to know when to stop looking for more
                            fileName=tokens_get_token(tokens,i+1);//file we work with should be after the bracket
                            if(arr[i][j]=='>')//found left bracket
                            {
                                close(1);// close stdout
                                newfd = open(fileName, O_CREAT|O_TRUNC|O_WRONLY, 0644);//create a clear write only file(0644 mode to save file correctly)
                            }
                            else
                            {
                                close(0);// close stdin
                                newfd = open(fileName, O_RDONLY); //tries to open readonly file
                            }
                            dup(newfd);//puts stdin or out to newfd
                            arr[i]=tokens_get_token(tokens,i+2);//replace bracket with word after file name if exists
                            break;

                        }
                    }
                }
               else
                    arr[i]=tokens_get_token(tokens,i+2);//keep taking words after the filename

            }
            arr[tokensLength]=NULL;//The array of pointers must be terminated by a null pointer.



          char *path=getenv("PATH");//Gets PATH var addres

            changeCharAtoB(':',' ',path);//changes PATH ':' to ' ' for tokenize
            struct tokens *tokePath = tokenize(path);//getting all pathes into a token
            char *arg1=tokens_get_token(tokens,0);//ponting to first arg so we can use it for building a path
            changeCharAtoB(' ',':',path);//changing PATH back to normal to avoid errors *such as using man

            ////////
            execv(arr[0],arr); //trying to execute fuul path or local programs
            ////////
            arr[0]=(char *)malloc((strlen(path)+strlen(arr[0]))*sizeof(char));//Allocating as big as space that arr[0] migt need

            for (unsigned int i=0;i < tokens_get_length(tokePath);i++)
            {
                execv(arr[0],arr); //trying to execute
                strcpy(arr[0],tokens_get_token(tokePath,i));//copy path i to arr[0]
                strcat(arr[0],"/");//adding / to to arr[0]
                strcat(arr[0],arg1);//addong first arg to arr[0]
            }
            if(execv(arr[0],arr) == -1)//trying to execute last time if got error print error
                {
                    perror(arg1);//using first arg for error
                }
            exit(EXIT_FAILURE);//exit son on failure
        } else {/*waiting for child to die*/
            do {
                wpid = waitpid(pid, &status, WUNTRACED);
            }while (!WIFEXITED(status) && !WIFSIGNALED(status));
            //fprintf(stdout, "This shell doesn't know how to run programs.\n");
        }
    }
    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);
    /* Clean up memory */
    tokens_destroy(tokens);
  }

return 0;
}
