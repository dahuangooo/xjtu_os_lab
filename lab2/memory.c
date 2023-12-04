#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


#define PROCESS_NAME_LEN 32 /*进程名长度*/
#define MIN_SLICE 10 /*最小碎片大小*/
#define DEFAULT_MEM_SIZE 1024 /*内存大小*/
#define DEFAULT_MEM_START 0 /*起始位置*/
/* 内存分配算法 */
#define MA_FF 1
#define MA_BF 2
#define MA_WF 3
int mem_size=DEFAULT_MEM_SIZE; /*内存大小*/
int ma_algorithm = MA_FF; /*当前分配算法*/
static int pid = 0; /*初始pid*/
int flag = 0; /*设置内存大小标志*/


/*描述每一个空闲块的数据结构*/ 
struct free_block_type{
    int size;
    int start_addr;
    struct free_block_type *next;
}; 
/*指向内存中空闲块链表的首指针*/ 
struct free_block_type *free_block;

/*每个进程分配到的内存块的描述*/ 
struct allocated_block{
    int pid; 
    int size; // 内存块大小
    int proc_len; // 实际进程大小
    int start_addr;
    char process_name[PROCESS_NAME_LEN]; 
    struct allocated_block *next;
};
/*进程分配内存块链表的首指针*/
struct allocated_block *allocated_block_head = NULL;


// 函数声明
struct free_block_type* init_free_block(int mem_size);
void display_menu();
int set_mem_size();
void set_algorithm();
void rearrange(int algorithm);
void rearrange_FF();
void rearrange_BF();
void rearrange_WF();
void rearrange_ab();
int new_process();
int allocate_mem(struct allocated_block *ab);
struct allocated_block *find_process(int pid);
void kill_process();
int free_mem(struct allocated_block *ab);
int dispose(struct allocated_block *free_ab);
int display_mem_usage();
struct free_block_type *swap_fr(struct free_block_type *p);
struct allocated_block *swap_ab(struct allocated_block *p);
void do_exit();
void clearBuffer();

struct free_block_type *swap_fr(struct free_block_type *p);
struct allocated_block *swap_ab(struct allocated_block *p);

int main(){
    char choice; 
    pid=0;
    free_block = init_free_block(mem_size); //初始化空闲区
    while(1) {
        display_menu(); //显示菜单
        clearBuffer();  // 清空输入缓冲区
        fflush(stdin);
        choice=getchar(); //获取用户输入
        switch(choice){
        case '1': set_mem_size(); break; //设置内存大小
        case '2': set_algorithm();flag=1; break;//设置算法
        case '3': new_process(); flag=1; break;//创建新进程
        case '4': kill_process(); flag=1; break;//删除进程
        case '5': display_mem_usage(); flag=1; break; //显示内存使用
        case '0': do_exit(); exit(0); //释放链表并退出
        default: break; 
        } 
    } 
}
/*初始化空闲块，默认为一块，可以指定大小及起始地址*/
struct free_block_type* init_free_block(int mem_size){
    struct free_block_type *fb;
    fb=(struct free_block_type *)malloc(sizeof(struct free_block_type));
    if(fb==NULL){
        printf("No mem\n");
        return NULL;
    }
    fb->size = mem_size;
    fb->start_addr = DEFAULT_MEM_START;
    fb->next = NULL;
    return fb;
}
/*显示菜单*/
void display_menu(){
    printf("\n");
    printf("1 - Set memory size (default=%d)\n", DEFAULT_MEM_SIZE);
    printf("2 - Select memory allocation algorithm\n");
    printf("3 - New process \n");
    printf("4 - Terminate a process \n");
    printf("5 - Display memory usage \n");
    printf("0 - Exit\n");
}
/*设置内存的大小*/
int set_mem_size(){
    int size;
    if(flag!=0){ //防止重复设置
        printf("Cannot set memory size again\n");
    return 0;
    }
    printf("Total memory size =");
    scanf("%d", &size);
    if(size>0) {
        mem_size = size;
        free_block->size = mem_size;
    }
    flag=1; 
    return 1;
}
/* 设置当前的分配算法 */
void set_algorithm(){
    int algorithm;
    printf("\t1 - First Fit\n");
    printf("\t2 - Best Fit \n");
    printf("\t3 - Worst Fit \n");
    scanf("%d", &algorithm);
    if(algorithm>=1 && algorithm <=3) 
        ma_algorithm=algorithm;
    //按指定算法重新排列空闲区链表
    rearrange(ma_algorithm); 
}
/*按指定的算法整理内存空闲块链表*/
void rearrange(int algorithm){
    switch(algorithm){
    case MA_FF: rearrange_FF(); break;
    case MA_BF: rearrange_BF(); break;
    case MA_WF: rearrange_WF(); break;
    }
}
/*按 FF 算法重新整理内存空闲块链表*/
void rearrange_FF(){ 
    //按照地址从小到大排列
    if(!free_block) return ; //保证空闲块链不为空
    int count = 1;
    struct free_block_type* p;
    p = free_block;
    while(p->next != NULL){ 
        count++;
        p = p->next;
    } // 统计共有几个空闲块
    for(int i = 0; i< count-1; i++){
        p = free_block;
        for(int j = 0; j< count-1-i;j++){
            if(p->start_addr > p->next->start_addr) swap_fr(p);
            p = p->next;
        }
    } // 冒泡排序
}
/*按 BF 算法重新整理内存空闲块链表*/
void rearrange_BF(){
    //按照大小从小到大排列
    if(!free_block) return ; //保证空闲块链不为空
    int count = 1;
    struct free_block_type* p;
    p = free_block;
    while(p->next != NULL){ 
        count++;
        p = p->next;
    } // 统计共有几个空闲块
    for(int i = 0; i< count-1; i++){
        p = free_block;
        for(int j = 0; j< count-1-i;j++){
            if(p->size > p->next->size) swap_fr(p);
            p = p->next;
        }
    } // 冒泡排序
}
/*按 WF 算法重新整理内存空闲块链表*/
void rearrange_WF(){
    //按照大小从大到小排列
    if(!free_block) return ; //保证空闲块链不为空
    int count = 1;
    struct free_block_type* p;
    p = free_block;
    while(p->next != NULL){ 
        count++;
        p = p->next;
    } // 统计共有几个空闲块
    for(int i = 0; i< count-1; i++){
        p = free_block;
        for(int j = 0; j< count-1-i;j++){
            if(p->size < p->next->size) swap_fr(p);
            p = p->next;
        }
    } // 冒泡排序
}
void rearrange_ab(){ 
    //对已分配块，按照地址从小到大排列
    if(!allocated_block_head) return ; //保证分配块链不为空
    int count = 1;
    struct allocated_block* p;
    p = allocated_block_head;
    while(p->next != NULL){ 
        count++;
        p = p->next;
    } // 统计共有几个分配块
    for(int i = 0; i< count-1; i++){
        p = allocated_block_head;
        for(int j = 0; j< count-1-i;j++){
            if(p->start_addr > p->next->start_addr) swap_ab(p);
            p = p->next;
        }
    } // 冒泡排序
}
/*创建新的进程，主要是获取内存的申请数量*/
int new_process(){
    struct allocated_block *ab;
    int size; int ret;
    ab=(struct allocated_block *)malloc(sizeof(struct allocated_block)); // 创建新的分配块
    if(!ab) exit(-5);

    ab->next = NULL;
    pid++;
    sprintf(ab->process_name, "PROCESS-%02d", pid);
    ab->pid = pid; 
    printf("Memory for %s:", ab->process_name);
    scanf("%d", &size);
    if(size>0) {
        ab->size = size;
        ab->proc_len = size;
    }
    ret = allocate_mem(ab); /* 从空闲区分配内存，ret==1 表示分配 ok*/

    /*如果此时 allocated_block_head 尚未赋值，则赋值*/
    if((ret==1) &&(allocated_block_head == NULL)){ 
        allocated_block_head=ab;
        return 1; 
    }
    /*分配成功，将该已分配块的描述插入已分配链表*/
    else if (ret==1) {
        ab->next=allocated_block_head;
        allocated_block_head=ab; // 放在最前面
        return 2; 
    }
    else if(ret==-1){ /*分配不成功*/
        printf("Allocation fail\n");
        free(ab);
        return -1; 
    }
    return 3;
}
/*分配内存模块*/
int allocate_mem(struct allocated_block *ab){
    struct free_block_type *fbt, *pre;
    int request_size=ab->size;
    fbt = pre = free_block;
    pre->next = NULL;
    //根据当前算法在空闲分区链表中搜索合适空闲分区进行分配，分配时注意以下情况：
    // 1. 找到可满足空闲分区且分配后剩余空间足够大，则分割
    // 2. 找到可满足空闲分区且但分配后剩余空间比较小，则一起分配
    // 3. 找不可满足需要的空闲分区但空闲分区之和能满足需要，则采用内存紧缩技术，进行空闲分区的合并，然后再分配
    // 4. 在成功分配内存后，应保持空闲分区按照相应算法有序
    // 5. 分配成功则返回 1，否则返回-1
    /*按照当前算法对空闲块进行排列*/
    rearrange(ma_algorithm);

    /*先寻找是否有可满足分区*/
    int flag = 0; // 是否成功找到可满足分区的标志位
    int shrink = 0;
    while (fbt)
    {
        shrink += fbt->size; // 记录总共的空闲区大小，为之后内存紧缩技术做准备
        if(fbt != free_block) pre->next = fbt;
        if(fbt->size < request_size){
            pre = pre->next;
            fbt = fbt->next;    
        }
        else{
            flag = 1; //成功找到
            break;
        } 
    } 
    
    /*找到后，寻找相应的分配方式*/
    int dif = fbt->size - request_size; // 产生碎片大小
    if(flag == 0){                          /*未找到，尝试内存紧缩*/
        if(shrink < request_size) return -1;    /*剩余内存总数都不满足分配，即：分配失败*/
        else{                                   /*可以进行内存紧缩*/
            rearrange_FF();
            rearrange_ab();
            /*重新分配进程块*/
            struct allocated_block *p_ab = allocated_block_head->next; 
            struct allocated_block *pre_ab = allocated_block_head; 
            int start_addr = DEFAULT_MEM_START;
            pre_ab->start_addr = start_addr;
            while(p_ab){
                p_ab->start_addr = start_addr + pre_ab->size;
                start_addr = p_ab->start_addr;
                p_ab = p_ab->next;
                pre_ab = pre_ab->next;
            } // 把进程块地址连在一起
            start_addr += pre_ab->size;
            //display_mem_usage();
            /*重新分配freeblock*/ 
            if(!free_block) return -1; //没有可分配空间了
            struct free_block_type *p_fr = free_block->next;
            struct free_block_type *pre_fr = free_block;
            free_block->size = shrink;
            free_block->start_addr = start_addr; // 空闲块跟在进程块后面
            while(p_fr){ // 把原本的空闲块都释放掉，防止内存泄漏
                struct free_block_type *fr = p_fr;
                p_fr = p_fr->next;
                free(fr);
            }
            free_block->next = NULL; // 只剩一个内存块
            return 1;
        }
    }                           
    else{                                   /*找到满足分区，进行分配*/
        if(dif >= MIN_SLICE){                   /*分配后剩余空间足够大，分割*/
            ab->start_addr = fbt->start_addr;
            fbt->start_addr += ab->size;
            fbt->size -= ab->size;
            return 1;
        }
        else if(dif < MIN_SLICE && dif >0){     /*分配后剩余空间比较小，一起分配*/
            ab->start_addr = fbt->start_addr;
            ab->size = fbt->size;
            if(fbt == free_block)
            free_block = fbt->next;
            return 1;
        }
    }
}
/*查找进程*/
struct allocated_block *find_process(int pid){
    struct allocated_block *p = allocated_block_head;
    while (p)
    {
        if(p->pid == pid)return p;
        p = p->next; 
    }
    return NULL;
}
/*删除进程，归还分配的存储空间，并删除描述该进程内存分配的节点*/
void kill_process(){
    struct allocated_block *ab;
    int pid;
    printf("Kill Process, pid=");
    scanf("%d", &pid);
    ab=find_process(pid);
    if(ab!=NULL){
        free_mem(ab); /*释放 ab 所表示的分配区*/
        dispose(ab); /*释放 ab 数据结构节点*/
    }
}
/*将 ab 所表示的已分配区归还，并进行可能的合并*/
int free_mem(struct allocated_block *ab){
    int algorithm = ma_algorithm;
    struct free_block_type *fbt, *pre, *work;
    fbt=(struct free_block_type*) malloc(sizeof(struct free_block_type));
    if(!fbt) return -1;
    // 进行可能的合并，基本策略如下
    // 1. 将新释放的结点插入到空闲分区队列末尾
    // 2. 对空闲链表按照地址有序排列
    // 3. 检查并合并相邻的空闲分区
    // 4. 将空闲链表重新按照当前算法排序
    fbt->size = ab->size;
    fbt->start_addr = ab->start_addr;
    fbt->next = free_block;
    free_block = fbt;
    rearrange_FF();
    pre = free_block;
    work = free_block->next;
    while (work)
    {
        if(pre->start_addr + pre->size >= work->start_addr){
            pre->size = (work->start_addr - pre->start_addr + work->size);
            pre->next = work->next;
            struct free_block_type* p = work;
            work = pre->next;
            free(p); // 释放合并后的空闲块
        } 
        else{
            pre = work;
            work = work->next;
        }
    }

    rearrange(algorithm); // 按当前算法排序
    return 1;
}
/*释放 ab 数据结构节点*/
int dispose(struct allocated_block *free_ab){
    struct allocated_block *pre, *ab;
    if(free_ab == allocated_block_head) { /*如果要释放第一个节点*/
        allocated_block_head = allocated_block_head->next;
        free(free_ab);
        return 1;
    }
    else{/*不是第一个节点*/
        pre = allocated_block_head; 
        ab = allocated_block_head->next;
        while(ab!=free_ab){ pre = ab; ab = ab->next; }//查找ab的位置
        pre->next = ab->next;
        free(ab);
        return 2;
    }
}
/* 显示当前内存的使用情况，包括空闲区的情况和已经分配的情况 */
int display_mem_usage(){
    struct free_block_type *fbt=free_block;
    struct allocated_block *ab=allocated_block_head;
    if(fbt==NULL) return(-1);
    printf("----------------------------------------------------------\n");
    /* 显示空闲区 */
    printf("Free Memory:\n");
    printf("%20s %20s\n", " start_addr", " size");
    while(fbt!=NULL){
        printf("%20d %20d\n", fbt->start_addr, fbt->size);
        fbt=fbt->next;
    } 
    /* 显示已分配区 */
    printf("\nUsed Memory:\n");
    printf("%10s %20s %10s %10s\n", "PID", "ProcessName", "start_addr", " size");
    while(ab!=NULL){
        printf("%10d %20s %10d %10d\n", ab->pid, ab->process_name, 
        ab->start_addr, ab->size);
        ab=ab->next;
    }
    printf("----------------------------------------------------------\n");
    return 0;
}
/*交换*/
struct free_block_type *swap_fr(struct free_block_type *p){
    int temp_size, temp_addr;

    temp_size = p->size;
    p->size = p->next->size;
    p->next->size = temp_size;

    temp_addr = p->start_addr;
    p->start_addr = p->next->start_addr;
    p->next->start_addr = temp_addr;
}
struct allocated_block *swap_ab(struct allocated_block *p){
    int temp_size, temp_addr, temp_proc_len, temp_pid;
    char temp_name[PROCESS_NAME_LEN];

    temp_size = p->size;
    p->size = p->next->size;
    p->next->size = temp_size;

    temp_addr = p->start_addr;
    p->start_addr = p->next->start_addr;
    p->next->start_addr = temp_addr;

    temp_proc_len = p->proc_len;
    p->proc_len = p->next->proc_len;
    p->next->proc_len = temp_proc_len;

    temp_pid = p->pid;
    p->pid = p->next->pid;
    p->next->pid = temp_pid;

    strcpy(temp_name, p->process_name);
    strcpy(p->process_name, p->next->process_name);
    strcpy(p->next->process_name, temp_name);
}
/*退出*/
void do_exit()
{
    struct free_block_type *fbt = free_block;
    struct allocated_block *ab = allocated_block_head;
    while(fbt){
        struct free_block_type *p = fbt;
        fbt = fbt->next;
        free(p);
    } 
    while(ab){
        struct allocated_block *p = ab;
        ab = ab->next;
        free(p);
    }
}
void clearBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}