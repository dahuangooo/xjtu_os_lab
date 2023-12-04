//
//  
//
//  Created by 杜昊阳 on 2023/10/15.
//

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int i = 1;
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
        i++;
        pid1 = getpid();
//      printf("child: pid = %d",pid);/* A */
//      printf("child: pid1 = %d/n",pid1);/* B */
        printf("child: value = %d  ",i);
        printf("child: val_address = %d\n",&i);
    }
    else
    {
        /* parent process */
        i--;
        pid1 = getpid();
//      printf("parent: pid = %d",pid);/* C */
//      printf("parent: pid1 = %d/n",pid1);/* D */
        printf("parent: value = %d  ",i);
        printf("parent: val_address = %d\n",&i);

        wait(NULL);
    }
    return 0;
}
