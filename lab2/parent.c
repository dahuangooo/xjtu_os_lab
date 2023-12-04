#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

void main()
{
    printf("quit process pid : %d\n", wait(0));
    printf("quit process pid : %d\n", wait(0)); //等待两个子进程结束
    printf("\nParent process is killed!!\n");
    exit(0); 
}