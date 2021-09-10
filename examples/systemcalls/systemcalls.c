#include "systemcalls.h"
#include <errno.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the dmd
 *   and return a boolean true if the system() call completed with success 
 *   or false() if it returned a failure
*/
    int sys=0;

    sys=system(cmd);
//	printf("sys value=======%d",sys);
    if(sys==0) {
	    return true;
    }

    return false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the 
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
	printf("----------do_exec start\n");
    va_list args;
    va_start(args, count);
    char * command[count+1];
    char * argument[count];
    pid_t fork_ret;
    int wait_ret=0;
    int i, status;
    int j=0, k=1;

    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    for(j=0; j<count-1; j++) {
	argument[j] = command[k];
	k++;
    }
//    command[count] = NULL;
    argument[count-1]=NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
   // command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *   
*/

    fork_ret = fork();
    printf("fork_ret=%d\n",fork_ret);
    if(fork_ret==-1) {

	    return false;
    }

    else if(fork_ret==0) {
	     execv(command[0], argument);
	     exit(-1);
    }

    wait_ret = waitpid(fork_ret, &status, 0);
    printf("wait_ret=%d\n",wait_ret);
    if(wait_ret==-1) {
	   printf("---------Wait failed\n");
	   return false;
    } 

    
    else if(WIFEXITED(status)) {
	    printf("--------Return exit status\n");
	    printf("status=%d\n",status);
	    int exit_status=WEXITSTATUS(status);
	    printf("exit_status=%d\n",exit_status);
	    if(exit_status!=0)
		    return false;
	    // return !WEXITSTATUS(status);
    }

    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.  
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *   
*/

    va_end(args);
    
    return true;
}
