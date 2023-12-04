#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>
int pid1,pid2; // 定义两个进程变量 
int main( ) {
    int fd[2];
    int ret;
    char InPipe[4001]; // 定义读缓冲区
    char c1='1', c2='2';
    while((ret = pipe(fd)) == -1); // 创建管道

    while((pid1 = fork( )) == -1); // 如果进程 1 创建不成功,则空循环
    if(pid1 == 0) { // 子进程1(写)
        lockf(fd[1],0,0); // 锁定管道
        for(int i = 0; i<2000; i++) {
            ssize_t bytesRead1 = write(fd[1], &c1, sizeof(c1));
        } //分2000次每次向管道写入字符’1’ 
        sleep(5);// 等待读进程读出数据
        lockf(fd[1],1,0); // 解除管道的锁定
        exit(0); // 结束进程1    
    }
    else { //父进程
        while((pid2 = fork()) == -1);// 若进程 2 创建不成功,则空循环
        if(pid2 == 0) { //子进程2（写）
            lockf(fd[1],0,0);
            for(int i = 0; i<2000; i++) {
                ssize_t bytesRead2 = write(fd[1], &c2, sizeof(c2));
            } //分2000次每次向管道写入字符’2’
            sleep(5); // 等待读进程读出数据
            lockf(fd[1],1,0); // 解除管道的锁定
            exit(0); // 结束进程2
        }
        else { //父进程（读）
            wait(0);// 等待子进程 1 结束
            wait(0);// 等待子进程 2 结束
            lockf(fd[0],0,0);
            read(fd[0], InPipe, sizeof(InPipe)-1);// 从管道中读出 4000 个字符
            lockf(fd[0],1,0);
            InPipe[4000] = '\0'; // 加字符串结束符 
            printf("%s\n",InPipe);// 显示读出的数据 
            exit(0);// 父进程结束
        } 
    }
    return 0;
}
     