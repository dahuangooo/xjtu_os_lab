#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
int i = 0;
void thread1(){
    int n;
    for(n=0;n=5000;n++){i++;}
}
void thread2(){
    int n;
    for(n=0;n=5000;n++){i++;}
}

int main() {
    pthread_t TID1,TID2;
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
//    int wait1 = pthread_join(TID1, NULL);
//    int wait2 = pthread_join(TID2, NULL);//这两个函数保证两个线程都执行完
    //检查两个线程是否创建成功
    printf("variable result: %d\n", i);
    return 0;
}
