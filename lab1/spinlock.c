/**
*spinlock.c
*in xjtu
*2023.8
*/
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

int i = 0;
// 定义自旋锁结构体 
typedef struct {
    int flag;
} spinlock_t;

// 初始化自旋锁
void spinlock_init(spinlock_t *lock) {
    lock->flag = 0;
}
// 获取自旋锁
void spinlock_lock(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->flag, 1)) {}// 自旋等待 
}
// 释放自旋锁
void spinlock_unlock(spinlock_t *lock) {
    __sync_lock_release(&lock->flag);
}
// 共享变量
int shared_value = 0;
// 线程函数
void *thread_function(void *arg) {
    spinlock_t *lock = (spinlock_t *)arg;
    for (int i; i < 5000; ++i) {
        spinlock_lock(lock);
        shared_value++;
        spinlock_unlock(lock);
    }
}
int main() {
    pthread_t thread1, thread2;
    spinlock_t lock;
    printf("Shared value: %d\n",shared_value);// 输出共享变量的值
    spinlock_init(&lock);// 初始化自旋锁
    // 创建两个线程
    int thr1 = pthread_create(&thread1, NULL, (void *)thread_function, &lock);
    if(thr1 != 0){
        printf ("Create thread1 error!\n");
        exit (1);
    }
    else{printf("thread1 create success!\n");}
    int thr2 = pthread_create(&thread2, NULL, (void *)thread_function, &lock);
    if(thr2 != 0){
        printf ("Create thread2 error!\n");
        exit (1);
    }
    else{printf("thread2 create success!\n");}
    // 等待线程结束
    int wait1 = pthread_join(thread1, NULL);
    int wait2 = pthread_join(thread2, NULL);//这两个函数保证两个线程都执行完
    printf("Shared value: %d\n",shared_value);// 输出共享变量的值
    return 0;
}
