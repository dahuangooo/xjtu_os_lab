#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

int i = 0;
pthread_mutex_t mutex;
void thread1(int i){
    int n;
    pid_t pid = getpid();
    for(n=0;n<10000;n++){
        pthread_mutex_lock (&mutex);
        i++;
        pthread_mutex_unlock (&mutex);
    }
    printf("thread1 tid = %ld ,pid =%d \n", (long int)syscall(SYS_gettid), pid);
    const char *path = "system_call";
    execl(path, path, NULL);
    perror("execl");
    printf("thread1 systemcall return\n");
}
void thread2(int i){
    int n;
    pid_t pid = getpid();
    for(n=0;n<10000;n++){
        pthread_mutex_lock (&mutex);
        i--;
        pthread_mutex_unlock (&mutex);
    }
    printf("thread2 tid = %ld ,pid = %d \n", (long int)syscall(SYS_gettid), pid);
    const char *path = "system_call";
    execl(path, path, NULL);
    perror("execl");
    printf("thread2 systemcall return\n");
}


int main() {
    pthread_t TID1,TID2;
    pthread_mutex_init(&mutex, NULL);
    int i = 100;
    int thr1 = pthread_create(&TID1, NULL, (void *)thread1, NULL);
    if(thr1 != 0){
        printf ("Create thread1 error!\n");
        exit (1);
    }
    else{printf("thread1 create success!\n");}
    int thr2 = pthread_create(&TID2, NULL, (void *)thread2, NULL);
    if(thr2 != 0){
        printf ("Create thread2 error!\n");
        exit (1);
    }
    else{printf("thread2 create success!\n");}
    int wait1 = pthread_join(TID1, NULL);
    int wait2 = pthread_join(TID2, NULL);//这两个函数保证两个线程都执行完
    //检查两个线程是否创建成功
    printf("variable result: %d\n", i);
    return 0;
}
