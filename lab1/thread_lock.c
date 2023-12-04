#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include<semaphore.h>
#include <stdlib.h>

// // 定义信号量结构体 
// struct Semaphore{
//     int* p;
//     int value;
// };
// struct Semaphore signal = {0, 1};

// void changethr(int* thread1,int* thread2){
//     if(signal.p = thread1){signal.p = thread2;}
//     else if(signal.p = thread2){signal.p = thread1;}
// }

// //定义PV操作
// void P(){
//     signal.value--;
// }
// void V(){
//     signal.value++;
// }

int i = 0;
pthread_mutex_t mutex;
void thread1(int i){
    int n;
    for(n=0;n<10000;n++){
        pthread_mutex_lock (&mutex);
        i++;
        pthread_mutex_unlock (&mutex);
    }
}
void thread2(int i){
    int n;
    for(n=0;n<10000;n++){
        pthread_mutex_lock (&mutex);
        i--;
        pthread_mutex_unlock (&mutex);
    }
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
    int wait1 = pthread_join(TID1, NULL);
    int wait2 = pthread_join(TID2, NULL);//这两个函数保证两个线程都执行完
    //检查两个线程是否创建成功
    printf("variable result: %d\n", i);
    return 0;
}

