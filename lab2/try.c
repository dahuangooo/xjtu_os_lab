#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
int flag; //为1时继续等待，为0时等待结束

void waiting();
void stop();

int main()
{
    flag = 1;
    alarm(5);
    signal(SIGINT, stop); 
    signal(SIGQUIT, stop); 
    signal(SIGALRM, stop); //开始接收中断
    pid_t pid1 = -1, pid2 = -1;

    while (pid1 == -1)
        pid1 = fork(); //创建子进程1
    if (pid1 > 0) //父进程运行
    {
        while (pid2 == -1)
            pid2 = fork(); //创建子进程2
        if (pid2 > 0) //父进程运行
        {
            waiting();
            //printf(" F: pid1 = %d,pid2 = %d \n\n",pid1,pid2); 
            kill(pid1,16);
            kill(pid2,17);
            wait(0);
            wait(0);
            printf("Parent process is killed!!\n");
            exit(0);
        }
        else //子进程2运行
        {
            // signal(SIGINT, SIG_IGN);
            // signal(SIGQUIT, SIG_IGN);
            // signal(SIGALRM, SIG_IGN); //屏蔽信号，防止覆盖
            signal(17,stop); //等待中断信号17
            pause();
            //printf(" C2: pid1 = %d,pid2 = %d \n",pid1,pid2);
            printf("Child process2 is killed by parent!!\n\n");
            exit(0); //释放资源，退出
        }
    }
    else //子进程1运行
    {
        // signal(SIGINT, SIG_IGN);
        // signal(SIGQUIT, SIG_IGN);
        // signal(SIGALRM, SIG_IGN);
        signal(16,stop); //等待中断信号16
        pause();
        //printf(" C1: pid1 = %d,pid2 = %d \n",pid1,pid2);
        printf("Child process1 is killed by parent!!\n\n");
        exit(0); //释放资源，退出
    }
    return 0;
}

void waiting()
{
    while ((flag!=0)){}; //循环等待，阻塞住子进程，让子进程等待父进程信号
}

void stop(int sig)
{
    flag = 0; //进程终止
    printf("\n%d stop test,pid = %d\n", sig, getpid());
}
