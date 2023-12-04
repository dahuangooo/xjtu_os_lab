//
//  
//
//  Created by 杜昊阳 on 2023/10/15.
//

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>

int main()
{
    pid_t pid,pid1;
    
    /* fork a child process */
    pid = fork();
    
    if(pid < 0)
    {
        /* error occurred */
        fprintf(stderr, "Fork Failed");
        return 1;
    }
    else if (pid == 0)
    {
        /* child process */
        pid1 = getpid();
        printf("child: pid = %d\n",pid1);/* A */
        const char *path = "system_call";
        execl(path, NULL);
        perror("execl");
        printf("child: pid1 = %d\n",pid1);/* B */
    }
    else
    {
        /* parent process */
        pid1 = getpid();
//        printf("parent: pid = %d\n",pid);/* C */
        printf("parent: pid = %d\n",pid1);/* D */
        wait(NULL);
    }
    
    return 0;
}
