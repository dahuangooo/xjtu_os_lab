#include <stdio.h>
#include "string.h"
#include "stdlib.h"
#include "time.h"
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>          

/* for STDIN_FILENO */
#define blocks 4611          // 1+1+1+512+4096,总块数
#define blocksiz 512         // 每块字节数
#define inodesiz 64          // 索引长度
#define data_begin_block 515 // 数据开始块
#define dirsiz 32            // 目录体长度
#define EXT2_NAME_LEN 15     // 文件名长度
#define PATH "vdisk"         // 文件系统

// 组描述符 68 字节
typedef struct ext2_group_desc
{                             
    char bg_volume_name[16];  // 卷名
    int bg_block_bitmap;      // 保存块位图的块号
    int bg_inode_bitmap;      // 保存索引结点位图的块号
    int bg_inode_table;       // 索引结点表的起始块号
    int bg_free_blocks_count; // 本组空闲块的个数
    int bg_free_inodes_count; // 本组空闲索引结点的个数
    int bg_used_dirs_count;   // 本组目录的个数
    char psw[16];
    char bg_pad[24]; // 填充(0xff)
} ext2_group_desc;
// 索引节点 64 字节
typedef struct ext2_inode
{                   
    int i_mode;     // 访问权限（1:r,2:w,3:rw）
    int i_type;     // 文件类型
    int i_blocks;   // 文件的数据块个数
    int i_size;     // 文件大小(字节)
    time_t i_atime; // 访问时间
    time_t i_ctime; // 创建时间
    time_t i_mtime; // 修改时间
    time_t i_dtime; // 删除时间
    int i_block[8]; // 指向数据块的指针，六个直接索引，一个一级子索引，一个二级子索引
    char i_pad[24]; // 填充 1(0xff)
} ext2_inode;
// 目录体 32 字节
typedef struct ext2_dir_entry
{                             
    int inode;                // 索引节点号
    int rec_len;              // 目录项长度
    int name_len;             // 文件名长度
    int file_type;            // 文件类型(1:普通文件，2:目录…)
    char name[EXT2_NAME_LEN]; // 文件名
    char dir_pad;             // 填充
} ext2_dir_entry;

/*定义全局变量*/
ext2_group_desc group_desc;        // 组描述符
ext2_inode inode;                  // 索引结点
ext2_dir_entry dir;                // 目录体
FILE *f;                           /*文件指针*/
unsigned int last_allco_inode = 0; // 上次分配的索引节点号
unsigned int last_allco_block = 0; // 上次分配的数据块号
int format_flag = 0;
/******************/

int getch()
{
    int ch;
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}

/********格式化文件系统**********/
/*
 * 初始化组描述符
 * 初始化数据块位图
 * 初始化索引节点位图
 * 初始化索引节点表 -添加一个索引节点
 * 第一个数据块中写入当前目录和上一目录
 */
int format()
{
    FILE *fp = NULL;
    int i;
    unsigned int zero[blocksiz / 4]; // 零数组，用来初始化块为 0
    time_t now; // 创建时间
    time(&now);
    while (fp == NULL)
        fp = fopen(PATH, "w+");

    // 清空所有目录项
    for (i = 0; i < blocks; i++)
    {
        fseek(fp, (data_begin_block + i) * blocksiz, SEEK_SET);
        fread(&dir, sizeof(ext2_dir_entry), 1, fp);
        // 读取目录项对应的inode
        fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
        fread(&inode, sizeof(ext2_inode), 1, fp);

        // 将inode设置为空白inode
        memset(&inode, 0, sizeof(ext2_inode));

        // 将清空后的inode写回文件
        fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
        fwrite(&inode, sizeof(ext2_inode), 1, fp);

        fwrite(&zero, blocksiz, 1, fp);
    }

    // 清空所有数据块
    for (i = 0; i < blocksiz / 4; i++)
        zero[i] = 0;
    for (i = 0; i < blocks; i++)
    { // 初始化所有 4611 块为 0
        fseek(fp, i * blocksiz, SEEK_SET);
        fwrite(&zero, blocksiz, 1, fp);
    }

    // 初始化组描述符
    strcpy(group_desc.bg_volume_name, "abcd"); // 初始化卷名为abcd
    group_desc.bg_block_bitmap = 1;            // 保存块位图的块号
    group_desc.bg_inode_bitmap = 2;            // 保存索引节点位图的块号
    group_desc.bg_inode_table = 3;             // 索引节点表的起始块号
    group_desc.bg_free_blocks_count = 4095;    // 除去一个初始化目录（根目录）
    group_desc.bg_free_inodes_count = 4095;
    group_desc.bg_used_dirs_count = 1;
    strcpy(group_desc.psw, "123");
    fseek(fp, 0, SEEK_SET);
    fwrite(&group_desc, sizeof(ext2_group_desc), 1, fp); // 第一块为组描述符

    // 初始化数据块位图和索引节点位图，第一位置为 1
    zero[0] = 0x80000000; 
    fseek(fp, 1 * blocksiz, SEEK_SET);
    fwrite(&zero, blocksiz, 1, fp); // 第二块为块位图，块位图的第一位为 1
    fseek(fp, 2 * blocksiz, SEEK_SET);
    fwrite(&zero, blocksiz, 1, fp); // 第三块为索引位图，索引节点位图的第一位为 1

    // 初始化索引节点表，添加一个索引节点
    inode.i_type = 2;
    inode.i_blocks = 1;
    inode.i_size = 64;
    inode.i_ctime = now;
    inode.i_atime = now;
    inode.i_mtime = now;
    inode.i_dtime = 0;
    inode.i_mode = 3;
    fseek(fp, 3 * blocksiz, SEEK_SET);
    fwrite(&inode, sizeof(ext2_inode), 1, fp); // 第四块开始为索引节点表

    // 向第一个数据块写 当前目录
    dir.inode = 0;
    dir.rec_len = 32;
    dir.name_len = 1;
    dir.file_type = 2;
    strcpy(dir.name, "."); // 当前目录
    fseek(fp, data_begin_block * blocksiz, SEEK_SET);
    fwrite(&dir, sizeof(ext2_dir_entry), 1, fp);

    // 当前目录之后写 上一目录
    dir.inode = 0;
    dir.rec_len = 32;
    dir.name_len = 2;
    dir.file_type = 2;
    strcpy(dir.name, ".."); // 上一目录
    fseek(fp, data_begin_block * blocksiz + dirsiz, SEEK_SET);
    fwrite(&dir, sizeof(ext2_dir_entry), 1, fp); // 第data_begin_block+1 =516 块开始为数据
    fclose(fp);
    format_flag = 1;
    return 0;
}

// 返回目录的起始存储位置，是在文件中的物理位置，每个目录 32 字节
int dir_entry_position(int dir_entry_begin, int i_block[8]) // dir_entry_begin目录体的相对开始字节，是在当前指向的iblockarray中的位置
{
    int dir_blocks = dir_entry_begin / 512;   // 存储目录之前的块数
    int block_offset = dir_entry_begin % 512; // 块内偏移字节数
    int a;
    FILE *fp = NULL;
    if (dir_blocks <= 5) // 前六个直接索引
        return data_begin_block * blocksiz + i_block[dir_blocks] * blocksiz + block_offset;
    else
    { // 间接索引
        while (fp == NULL)
            fp = fopen(PATH, "r+");
        dir_blocks = dir_blocks - 6;
        if (dir_blocks < 128)
        { // 一个块 512 字节，一个int为 4 个字节 一级索引有 512/4 =128 个， 即一级索引的第七个块可以满足
            int a;
            fseek(fp, data_begin_block * blocksiz + i_block[6] * blocksiz + dir_blocks * 4, SEEK_SET);  // 数据开始位置+一级索引开始位置+索引中存放块号的位置
            fread(&a, sizeof(int), 1, fp); // 读出储存实际起点的块号
            return data_begin_block * blocksiz + a * blocksiz + block_offset;
        }
        else // 二级索引
        { 
            dir_blocks = dir_blocks - 128;
            fseek(fp, data_begin_block * blocksiz + i_block[7] * blocksiz + (dir_blocks / 128) * 4, SEEK_SET);
            fread(&a, sizeof(int), 1, fp);
            fseek(fp, data_begin_block * blocksiz + a * blocksiz + dir_blocks % 128 * 4, SEEK_SET);
            fread(&a, sizeof(int), 1, fp);
            return data_begin_block * blocksiz + a * blocksiz + block_offset;
        }
        fclose(fp);
    }
}

/********打开一个目录**********/
/*在当前目录 打开一个目录，current指向新打开的当前目录（ext2_inode）*/
int Open(ext2_inode *current, char *name)
{
    FILE *fp = NULL;
    int i;
    while (fp == NULL)
        fp = fopen(PATH, "r+");
    for (i = 0; i < (current->i_size / 32); i++) // 循环遍历当前目录的所有目录项，每个目录项的大小为32字节
    {
        fseek(fp, dir_entry_position(i * 32, current->i_block), SEEK_SET);
        fread(&dir, sizeof(ext2_dir_entry), 1, fp); // 计算目录项在文件系统中的实际位置，并读取目录项的内容
        if (!strcmp(dir.name, name)) // 二者匹配
        {
            if (dir.file_type == 2) // 目录
            { 
                fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET); // 在索引结点表里找对应索引结点
                fread(current, sizeof(ext2_inode), 1, fp); // 给current重新赋值为找到的索引结点，即进入了要找的目录
                fclose(fp);
                return 0;
            }
        }
    }
    fclose(fp);
    return 1;
}
/********关闭当前目录**********/
/*关闭时仅修改最后访问时间 返回时 打开上一目录 作为当前目录*/
int Close(ext2_inode *current)
{
    time_t now;
    ext2_dir_entry bentry;
    FILE *fout;
    fout = fopen(PATH, "r+");
    time(&now);
    current->i_atime = now; // 修改最后访问时间
    fseek(fout, (data_begin_block + current->i_block[0]) * blocksiz, SEEK_SET); // 定位到当前目录项的位置
    fread(&bentry, sizeof(ext2_dir_entry), 1, fout); // current's dir_entry，记录目录项
    fseek(fout, 3 * blocksiz + (bentry.inode) * sizeof(ext2_inode), SEEK_SET);
    fwrite(current, sizeof(ext2_inode), 1, fout); // 定位到索引结点表中与当前目录项关联的索引结点的位置，将 current 中的索引结点写回文件系统
    fclose(fout);
    return Open(current, "..");
}
/********读取一个文件**********/
/*在目录'current'中读取文件'name'的内容，并打印*/
int Read(ext2_inode *current, int mode, char *name) {
    if( mode == 1 || mode == 3){
        FILE *fp = NULL;
        int i;
        
        while (fp == NULL)
            fp = fopen(PATH, "r+"); // 以读写模式打开文件系统文件

        for (i = 0; i < (current->i_size / 32); i++) {
            fseek(fp, dir_entry_position(i * 32, current->i_block), SEEK_SET); // 定位目录项的偏移位置
            fread(&dir, sizeof(ext2_dir_entry), 1, fp); // 读取目录项的内容

            if (!strcmp(dir.name, name)) { // 找到目标文件
                if (dir.file_type == 1) { // 如果是文件而不是目录
                    time_t now;
                    ext2_inode node;
                    char content_char;

                    fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
                    fread(&node, sizeof(ext2_inode), 1, fp); // 读取文件的原始索引结点

                    for (i = 0; i < node.i_size; i++) {
                        fseek(fp, dir_entry_position(i, node.i_block), SEEK_SET);
                        fread(&content_char, sizeof(char), 1, fp);
                        if (content_char == 0xD)
                            printf("\n");
                        else
                            printf("%c", content_char);
                    } // 遍历文件的数据块，逐字节读取并打印文件内容

                    printf("\n");

                    time(&now);
                    node.i_atime = now; // 更新文件的最后访问时间

                    fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
                    fwrite(&node, sizeof(ext2_inode), 1, fp); // 更新文件的索引结点

                    fclose(fp); // 关闭文件系统文件
                    return 0; // 返回0表示成功读取文件内容
                }
            }
        }

        fclose(fp); // 关闭文件系统文件
        return 1; // 返回1表示未找到指定的文件
    }
    else {
        printf("This file can't be read!");
        return 0;
    }
}

// 寻找空索引，将其取出，准备赋值
int FindInode() {
    FILE *fp = NULL;
    unsigned int zero[blocksiz / 4];
    int i;

    while (fp == NULL)
        fp = fopen(PATH, "r+"); // 以读写模式打开文件系统文件

    fseek(fp, 2 * blocksiz, SEEK_SET); // 定位到inode位图的位置
    fread(zero, blocksiz, 1, fp);      // 读取inode位图的内容

    for (i = last_allco_inode; i < (last_allco_inode + blocksiz / 4); i++) {
        if (zero[i % (blocksiz / 4)] != 0xffffffff) { // 如果该位图块还有可用的位
            unsigned int j = 0x80000000, k = zero[i % (blocksiz / 4)], l = i;

            for (i = 0; i < 32; i++) {
                if (!(k & j)) { // 找到一个可用的位
                    zero[l % (blocksiz / 4)] = zero[l % (blocksiz / 4)] | j;

                    group_desc.bg_free_inodes_count -= 1; // 更新组描述符中的空闲索引节点数
                    fseek(fp, 0, 0);
                    fwrite(&group_desc, sizeof(ext2_group_desc), 1, fp); // 更新组描述符
                    
                    fseek(fp, 2 * blocksiz, SEEK_SET);
                    fwrite(zero, blocksiz, 1, fp); // 更新inode位图
                    last_allco_inode = l % (blocksiz / 4);

                    fclose(fp);
                    return l % (blocksiz / 4) * 32 + i; // 返回找到的索引节点的编号
                } else {
                    j = j / 2;
                }
            }
        }
    }

    fclose(fp); // 关闭文件系统文件
    return -1; // 返回-1表示未找到可用的索引节点
}

// 寻找空block，思路同上
int FindBlock() {
    FILE *fp = NULL;
    unsigned int zero[blocksiz / 4];
    int i;

    while (fp == NULL)
        fp = fopen(PATH, "r+"); // 以读写模式打开文件系统文件

    fseek(fp, 1 * blocksiz, SEEK_SET); // 定位到块位图的位置
    fread(zero, blocksiz, 1, fp);      // 读取块位图的内容

    for (i = last_allco_block; i < (last_allco_block + blocksiz / 4); i++) {
        if (zero[i % (blocksiz / 4)] != 0xffffffff) { // 如果该位图块还有可用的位
            unsigned int j = 0X80000000, k = zero[i % (blocksiz / 4)], l = i;

            for (i = 0; i < 32; i++) {
                if (!(k & j)) { // 找到一个可用的位
                    zero[l % (blocksiz / 4)] = zero[l % (blocksiz / 4)] | j;

                    group_desc.bg_free_blocks_count -= 1; // 更新组描述符中的空闲块数
                    fseek(fp, 0, 0);
                    fwrite(&group_desc, sizeof(ext2_group_desc), 1, fp); // 更新组描述符
                    
                    fseek(fp, 1 * blocksiz, SEEK_SET);
                    fwrite(zero, blocksiz, 1, fp); // 更新块位图
                    last_allco_block = l % (blocksiz / 4);
                    
                    fclose(fp);
                    return l % (blocksiz / 4) * 32 + i; // 返回找到的数据块的编号
                } else {
                    j = j / 2;
                }
            }
        }
    }

    fclose(fp); // 关闭文件系统文件
    return -1; // 返回-1表示未找到可用的数据块
}

// 删除inode，更新inode节点位图
void DelInode(int len) { // len为索引结点的编号
    unsigned int zero[blocksiz / 4], i;
    int j;

    f = fopen(PATH, "r+"); // 以读写模式打开文件系统文件
    fseek(f, 2 * blocksiz, SEEK_SET); // 定位到inode位图的位置
    fread(zero, blocksiz, 1, f); // 读取inode位图的内容

    i = 0x80000000;
    for (j = 0; j < len % 32; j++)
        i / 2; // 右移

    if (zero != NULL && len / 32 < sizeof(zero) / sizeof(zero[0])) {
        zero[len / 32] = zero[len / 32] ^ i; // 将指定数据块位置的位取反
    } else {
        // 处理空指针或越界的情况
        // fprintf(stderr, "Error: Null pointer or index out of bounds\n");
    }

    fseek(f, 2 * blocksiz, SEEK_SET);
    fwrite(zero, blocksiz, 1, f); // 更新inode位图
    fclose(f); // 关闭文件系统文件
}

// 删除block块，更新块位图
void DelBlock(int len) { // len为block块的块号
    unsigned int zero[blocksiz / 4], i;
    int j;

    f = fopen(PATH, "r+"); // 以读写模式打开文件系统文件
    fseek(f, 1 * blocksiz, SEEK_SET); // 定位到块位图的位置
    fread(zero, blocksiz, 1, f); // 读取块位图的内容

    i = 0x80000000;
    for (j = 0; j < len % 32; j++)
        i = i / 2;

    if (zero != NULL && len / 32 < sizeof(zero) / sizeof(zero[0])) {
        zero[len / 32] = zero[len / 32] ^ i; // 将指定数据块位置的位取反
    } else {
        // 处理空指针或越界的情况
        // fprintf(stderr, "Error: Null pointer or index out of bounds\n");
    }
    fseek(f, 1 * blocksiz, SEEK_SET);
    fwrite(zero, blocksiz, 1, f); // 更新块位图
    fclose(f); // 关闭文件系统文件
}

// 向索引节点添加数据块，没有处理数据块的实际内容，只是处理了文件系统中索引节点的索引表
void add_block(ext2_inode *current, int i, int j) { // i为索引结点号，j为块号
    FILE *fp = NULL;

    while (fp == NULL)
        fp = fopen(PATH, "r+"); // 以读写模式打开文件系统文件
    // 如果是直接索引，直接将数据块号 j 赋值给对应的直接索引项
    if (i < 6) { // 直接索引
        current->i_block[i] = j;
    } 
    else {
        // 如果是一级索引，先找到或分配一级索引块，然后将数据块号 j 写入相应的位置
        i = i - 6;
        if (i == 0) {
            current->i_block[6] = FindBlock(); // 一级索引的第一个块
            fseek(fp, data_begin_block * blocksiz + current->i_block[6] * blocksiz, SEEK_SET);
            fwrite(&j, sizeof(int), 1, fp);
        } 
        else if (i < 128) { // 一级索引
            fseek(fp, data_begin_block * blocksiz + current->i_block[6] * blocksiz + i * 4, SEEK_SET);
            fwrite(&j, sizeof(int), 1, fp);
        } 
        // 如果是二级索引，先找到或分配二级索引块，然后找到一级索引块，并在其中找到或分配一级索引块，最后将数据块号 j 写入相应的位置
        else { // 二级索引
            i = i - 128;
            if (i == 0) {
                current->i_block[7] = FindBlock(); // 二级索引的第一个块
                fseek(fp, data_begin_block * blocksiz + current->i_block[7] * blocksiz, SEEK_SET);
                i = FindBlock();
                fwrite(&i, sizeof(int), 1, fp);
                fseek(fp, data_begin_block * blocksiz + i * blocksiz, SEEK_SET);
                fwrite(&j, sizeof(int), 1, fp);
            }
            if (i % 128 == 0) {
                fseek(fp, data_begin_block * blocksiz + current->i_block[7] * blocksiz + i / 128 * 4, SEEK_SET);
                i = FindBlock();
                fwrite(&i, sizeof(int), 1, fp);
                fseek(fp, data_begin_block * blocksiz + i * blocksiz, SEEK_SET);
                fwrite(&j, sizeof(int), 1, fp);
            } else {
                fseek(fp, data_begin_block * blocksiz + current->i_block[7] * blocksiz + i / 128 * 4, SEEK_SET);
                fread(&i, sizeof(int), 1, fp);
                fseek(fp, data_begin_block * blocksiz + i * blocksiz + i % 128 * 4, SEEK_SET);
                fwrite(&j, sizeof(int), 1, fp);
            }
        }
    }

    fclose(fp); // 关闭文件系统文件
}

// 为当前目录寻找一个空目录体
int FindEntry(ext2_inode *current) {
    FILE *fout = NULL;
    int location;       // 目录项的绝对地址
    int block_location; // 块的相对地址
    int temp;           // 每个block可以存放的目录项数量
    int remain_block;   // 剩余块数
    location = data_begin_block * blocksiz;
    temp = blocksiz / sizeof(int);
    fout = fopen(PATH, "r+");
    
    // 查看当前文件的大小是否会超出一个块
    if (current->i_size % blocksiz == 0) {
        add_block(current, current->i_blocks, FindBlock());
        current->i_blocks++;
    }

    // 前 6 个块直接索引
    if (current->i_blocks < 6) {
        location += current->i_block[current->i_blocks - 1] * blocksiz;
        location += current->i_size % blocksiz;
    }
    // 一级索引
    else if (current->i_blocks < temp + 5) {
        block_location = current->i_block[6];
        fseek(fout, (data_begin_block + block_location) * blocksiz + (current->i_blocks - 6) * sizeof(int), SEEK_SET);
        fread(&block_location, sizeof(int), 1, fout);
        location += block_location * blocksiz;
        location += current->i_size % blocksiz;
    }
    // 二级索引
    else {
        block_location = current->i_block[7];
        remain_block = current->i_blocks - 6 - temp;
        fseek(fout, (data_begin_block + block_location) * blocksiz + (int)((remain_block - 1) / temp + 1) * sizeof(int), SEEK_SET);
        fread(&block_location, sizeof(int), 1, fout);
        remain_block = remain_block % temp;
        fseek(fout, (data_begin_block + block_location) * blocksiz + remain_block * sizeof(int), SEEK_SET);
        fread(&block_location, sizeof(int), 1, fout);
        location += block_location * blocksiz;
        location += current->i_size % blocksiz + dirsiz;
    }

    // 更新当前索引节点的大小
    current->i_size += dirsiz;
    fclose(fout);
    return location;
}

/*********创建文件或者目录*********/
/*
 * type=1 创建文件 type=2 创建目录
 * current 当前目录索引节点
 * name 文件名或目录名
 */
int Create(int type,int mode, ext2_inode *current, char *name) {
    FILE *fout = NULL;
    int i;
    int block_location;     // 新块的位置
    int node_location;      // 新inode的位置
    int dir_entry_location; // 新目录项的位置

    time_t now;
    ext2_inode ainode;       // 新inode
    ext2_dir_entry aentry, bentry; // bentry保存当前系统的目录项信息
    time(&now);
    fout = fopen(PATH, "r+");

    if (strlen(name) > EXT2_NAME_LEN) {
        printf("Directory name is too long!\n");
        fclose(fout);
        return 1;
    }

    // [1] 寻找一个可用的inode
    node_location = FindInode();

    // [2] 遍历当前目录的所有目录项，检查是否已存在同名文件或目录
    for (i = 0; i < current->i_size / dirsiz; i++) {
        fseek(fout, dir_entry_position(i * sizeof(ext2_dir_entry), current->i_block), SEEK_SET);
        fread(&aentry, sizeof(ext2_dir_entry), 1, fout);
        if (aentry.file_type == type && !strcmp(aentry.name, name))
            return 1; // 如果已存在同名文件或目录，返回错误码
    }

    // [3] 读取当前目录的第一个目录项，用于后续操作
    fseek(fout, (data_begin_block + current->i_block[0]) * blocksiz, SEEK_SET);
    fread(&bentry, sizeof(ext2_dir_entry), 1, fout); // 当前目录的目录项

    // [4] 根据文件类型初始化新的inode
    if (type == 1) { // 文件
        ainode.i_type = 1;
        ainode.i_blocks = 0; // 文件暂无内容
        ainode.i_size = 0;   // 初始文件大小为 0
        ainode.i_atime = now;
        ainode.i_ctime = now;
        ainode.i_dtime = now;
        ainode.i_mode = mode;
    } 
    else { // 目录
        ainode.i_type = 2;   // 目录
        ainode.i_blocks = 1; // 目录当前和上一目录
        ainode.i_size = 64;  // 初始大小 32*2=64
        ainode.i_atime = now;
        ainode.i_ctime = now;
        ainode.i_dtime = now;
        ainode.i_mode = 3;

        // 初始化当前目录
        block_location = FindBlock();
        ainode.i_block[0] = block_location;
        fseek(fout, (data_begin_block + block_location) * blocksiz, SEEK_SET);
        // 写入"."目录项
        aentry.inode = node_location;
        aentry.rec_len = sizeof(ext2_dir_entry);
        aentry.name_len = 1;
        aentry.file_type = 2;
        strcpy(aentry.name, ".");
        aentry.dir_pad = 0;
        fwrite(&aentry, sizeof(ext2_dir_entry), 1, fout);
        // 写入".."目录项
        aentry.inode = bentry.inode;
        aentry.rec_len = sizeof(ext2_dir_entry);
        aentry.name_len = 2;
        aentry.file_type = 2;
        strcpy(aentry.name, "..");
        aentry.dir_pad = 0;
        fwrite(&aentry, sizeof(ext2_dir_entry), 1, fout);
        // 写入一个空目录项，共计14个，使数据块满
        aentry.inode = 0;
        aentry.rec_len = sizeof(ext2_dir_entry);
        aentry.name_len = 0;
        aentry.file_type = 0;
        aentry.name[EXT2_NAME_LEN] = 0;
        aentry.dir_pad = 0;
        fwrite(&aentry, sizeof(ext2_dir_entry), 14, fout); // 清空数据块
    }

    // [5] 保存新建inode
    fseek(fout, 3 * blocksiz + (node_location) * sizeof(ext2_inode), SEEK_SET);
    fwrite(&ainode, sizeof(ext2_inode), 1, fout);

    // [6] 将新建inode 的信息写入current 指向的数据块
    aentry.inode = node_location;
    aentry.rec_len = dirsiz;
    aentry.name_len = strlen(name);
    if (type == 1) {
        aentry.file_type = 1;
    } else {
        aentry.file_type = 2;
    }
    strncpy(aentry.name, name, EXT2_NAME_LEN);
    aentry.name[EXT2_NAME_LEN] = '\0'; // 设置字符串结尾
    aentry.dir_pad = 0;
    dir_entry_location = FindEntry(current);
    fseek(fout, dir_entry_location, SEEK_SET); // 定位条目位置
    fwrite(&aentry, sizeof(ext2_dir_entry), 1, fout);
    // [7] 保存current 的信息,bentry 是current 指向的block 中的第一条
    fseek(fout, 3 * blocksiz + (bentry.inode) * sizeof(ext2_inode), SEEK_SET);
    fwrite(current, sizeof(ext2_inode), 1, fout);
    fclose(fout);
    return 0;
}

/********向一个文件写入数据**********/
/*
 * write data to file 'name' in directory 'current'
 * if there isn't this file in this directory ,remaind create a new one
 */
// 写文件的函数，参数包括当前目录的inode（current）和文件名（name）。
// 通过终端输入，将内容写入文件。
// 返回值为整数，0表示写入成功。
// 没有设置覆写还是接着写，默认接着写
int Write(ext2_inode *current, int mode, char *name) {
    if(mode == 2 || mode == 3){ 
        FILE *fp = NULL;
        ext2_dir_entry dir;
        ext2_inode node;
        time_t now;
        char str;
        int i;

        // 打开文件，如果为空则循环尝试打开
        while (fp == NULL)
            fp = fopen(PATH, "r+");

        // 遍历当前目录的所有目录项，查找该文件
        while (1) {
            for (i = 0; i < (current->i_size / 32); i++) {
                fseek(fp, dir_entry_position(i * 32, current->i_block), SEEK_SET);
                fread(&dir, sizeof(ext2_dir_entry), 1, fp);
                if (!strcmp(dir.name, name)) {
                    if (dir.file_type == 1) {
                        fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
                        fread(&node, sizeof(ext2_inode), 1, fp);
                        break;
                    }
                }
            }
            if (i < current->i_size / 32) // 文件存在
                break;

            // 如果文件不存在，提示用户创建
            printf("There isn't this file, please create it first\n");
            return 0;
        }

        // 从终端输入内容，写入文件
        str = getch();
        while (str != 27) { // 27表示Esc键
            printf("%c", str);

            // 如果文件大小为512的倍数，增加一个块
            if (!(node.i_size % 512)) {
                add_block(&node, node.i_size / 512, FindBlock());
                node.i_blocks += 1;
            }

            // 写入字符到文件
            fseek(fp, dir_entry_position(node.i_size, node.i_block), SEEK_SET);
            fwrite(&str, sizeof(char), 1, fp);
            node.i_size += sizeof(char);

            // 如果输入回车，显示并写入换行符
            if (str == 0x0d)
                printf("%c", 0x0a);

            // 获取下一个字符
            str = getch();

            // 如果输入Esc键，退出循环
            if (str == 27)
                break;
        }

        // 更新inode的修改时间和访问时间
        time(&now);
        node.i_mtime = now;
        node.i_atime = now;

        // 将更新后的inode写回文件
        fseek(fp, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
        fwrite(&node, sizeof(ext2_inode), 1, fp);

        // 关闭文件
        fclose(fp);
        printf("\n");
        return 0;
    }
    else {
        return 1;
    }
}

/**********ls命令********/
/*
 * 列出当前目录的文件和目录
 */
// 列出当前目录下的所有文件和目录信息
void Ls(ext2_inode *current) {
    ext2_dir_entry dir;
    int i, j;
    char timestr[500];
    ext2_inode node;
    f = fopen(PATH, "r+");
    char mode[20];

    // 打印表头
    printf("Type\t\tFileName\tMODE\t\tCreateTime\t\t\tLastAccessTime\t\t\tModifyTime\n");

    if(format_flag == 1){
        current->i_size = 64;
        format_flag = 0;
    }
    // 遍历当前目录的所有目录项
    for (i = 0; i < current->i_size / 32; i++) {
        fseek(f, dir_entry_position(i * 32, current->i_block), SEEK_SET);
        fread(&dir, sizeof(ext2_dir_entry), 1, f);

        // 读取目录项对应的inode
        fseek(f, 3 * blocksiz + dir.inode * sizeof(ext2_inode), SEEK_SET);
        fread(&node, sizeof(ext2_inode), 1, f);

        // 将时间转换为字符串格式
        strcpy(timestr, "");
        // strcat(timestr, asctime(localtime(&node.i_ctime)));
        // strcat(timestr, asctime(localtime(&node.i_atime)));
        // strcat(timestr, asctime(localtime(&node.i_mtime)));

        strncat(timestr, asctime(localtime(&node.i_ctime)), sizeof(timestr) - strlen(timestr) - 1);
        strncat(timestr, asctime(localtime(&node.i_atime)), sizeof(timestr) - strlen(timestr) - 1);
        strncat(timestr, asctime(localtime(&node.i_mtime)), sizeof(timestr) - strlen(timestr) - 1);




        switch(node.i_mode){
            case 1:
                strcpy(mode,"--r---");
                break;
            case 2:
                strcpy(mode,"--w---");
                break;
            case 3:
                strcpy(mode,"--rw--");
                break;
            default:
                strcpy(mode,"------");
        }
        
        // 将字符串中的换行符替换为制表符
        for (j = 0; j < strlen(timestr) - 1; j++)
            if (timestr[j] == '\n')
                timestr[j] = '\t';

        // 根据文件类型打印信息
        if (dir.file_type == 1)
            printf("File     \t%s\t\t%s\t\t%s", dir.name, mode, timestr);
        else
            printf("Directory\t%s\t\t%s\t\t%s", dir.name, "--rw--", timestr);
    }

    // 关闭文件
    fclose(f);
}

// 初始化函数，读取当前目录的inode信息
int initialize(ext2_inode *cu) {
    f = fopen(PATH, "r+");
    
    // 定位到当前目录的inode位置，读取inode信息
    fseek(f, 3 * blocksiz, 0);
    fread(cu, sizeof(ext2_inode), 1, f);
    
    // 关闭文件
    fclose(f);
    
    return 0;
}

/*********修改文件系统密码*********/
/*
 * 修改成功返回 0
 * 修改不成功返回 1
 */
int Password() {
    char psw[16], ch[10];

    // 输入旧密码
    printf("Please input the old password\n");
    scanf("%s", psw);

    // 检查旧密码是否匹配
    if (strcmp(psw, group_desc.psw) != 0) {
        printf("Password error!\n");
        return 1;
    }

    while (1) {
        // 输入新密码
        printf("Please input the new password:");
        scanf("%s", psw);

        while (1) {
            // 提示用户确认是否修改密码
            printf("Modify the password?[Y/N]");
            scanf("%s", ch);

            if (ch[0] == 'N' || ch[0] == 'n') {
                // 用户取消修改密码
                printf("You canceled the modify of your password\n");
                return 1;
            } else if (ch[0] == 'Y' || ch[0] == 'y') {
                // 修改密码并写入磁盘
                strcpy(group_desc.psw, psw);
                f = fopen(PATH, "r+");
                fseek(f, 0, 0);
                fwrite(&group_desc, sizeof(ext2_group_desc), 1, f);
                fclose(f);
                return 0;
            } else {
                // 提示用户输入无效命令
                printf("Meaningless command\n");
            }
        }
    }
}

/*********登陆，验证文件系统密码*********/
int login()
{
    char psw[16];
    printf("please input the password(init:123):");
    scanf("%s", psw);
    return strcmp(group_desc.psw, psw);
}
/*********退出*********/
void exitdisplay()
{
    printf("Thank you for using~ Byebye!\n");
    return;
}
/**************初始化文件系统******************/
/*返回 1 初始化失败，返回 0 初始化成功*/
// 文件系统初始化函数
int initfs(ext2_inode *cu) {
    f = fopen(PATH, "r+");

    if (f == NULL) {
        char ch;
        int i;

        // 提示用户是否创建文件系统
        printf("File system couldn't be found. Do you want to create one?\n[Y/N]");

        i = 1;
        while (i) {
            scanf("%c", &ch);

            switch (ch) {
            case 'Y':
            case 'y':
                // 创建文件系统
                if (format() != 0)
                    return 1;

                f = fopen(PATH, "r");
                i = 0;
                break;

            case 'N':
            case 'n':
                // 用户选择不创建文件系统，退出程序
                exitdisplay();
                return 1;

            default:
                // 用户输入无效命令
                printf("Sorry, meaningless command\n");
                break;
            }
        }
    }

    // 读取组描述符和inode信息
    fseek(f, 0, SEEK_SET);
    fread(&group_desc, sizeof(ext2_group_desc), 1, f);
    fseek(f, 3 * blocksiz, SEEK_SET);
    fread(&inode, sizeof(ext2_inode), 1, f);
    fclose(f);

    // 初始化当前目录
    initialize(cu);

    return 0;
}

/*********获取当前目录的目录名*********/
void getstring(char *cs, ext2_inode node) {
    ext2_inode current = node;
    int i, j;
    ext2_dir_entry dir;
    f = fopen(PATH, "r+");

    // 打开上一级目录
    Open(&current, "..");

    // 在当前目录中查找当前目录的目录项的索引结点
    for (i = 0; i < node.i_size / 32; i++) {
        fseek(f, dir_entry_position(i * 32, node.i_block), SEEK_SET);
        fread(&dir, sizeof(ext2_dir_entry), 1, f);
        if (!strcmp(dir.name, ".")) {
            j = dir.inode;
            break;
        }
    }

    // 在上一级目录中查找当前目录对应索引结点，记录name
    for (i = 0; i < current.i_size / 32; i++) {
        fseek(f, dir_entry_position(i * 32, current.i_block), SEEK_SET);
        fread(&dir, sizeof(ext2_dir_entry), 1, f);
        if (dir.inode == j) {
            strcpy(cs, dir.name);
            fclose(f);
            return;
        }
    }

    fclose(f);
}

/*******在当前目录删除目录或者文件***********/
void DeleteFile(ext2_inode *file_inode) {
    FILE *fout = fopen(PATH, "r+");

    // 删除直接索引块
    for (int i = 0; i < 6; i++) {
        int block_location = file_inode->i_block[i];
        if (block_location != 0) {
            DelBlock(block_location);
            file_inode->i_blocks--;
        }
    }

    // 删除一级索引块
    if (file_inode->i_blocks > 0) {
        int block_location = file_inode->i_block[6];
        fseek(fout, (data_begin_block + block_location) * blocksiz, SEEK_SET);
        for (int i = 0; i < blocksiz / sizeof(int); i++) {
            if (file_inode->i_blocks == 0) {
                break;
            }
            int Blocation2;
            fread(&Blocation2, sizeof(int), 1, fout);
            DelBlock(Blocation2);
            file_inode->i_blocks--;
        }
        DelBlock(block_location);
    }

    // 删除二级索引块
    if (file_inode->i_blocks > 0) {
        int block_location = file_inode->i_block[7];
        for (int i = 0; i < blocksiz / sizeof(int); i++) {
            fseek(fout, (data_begin_block + block_location) * blocksiz + i * sizeof(int), SEEK_SET);
            int Blocation2;
            fread(&Blocation2, sizeof(int), 1, fout);
            fseek(fout, (data_begin_block + Blocation2) * blocksiz, SEEK_SET);
            for (int k = 0; k < blocksiz / sizeof(int); k++) {
                if (file_inode->i_blocks == 0) {
                    break;
                }
                int Blocation3;
                fread(&Blocation3, sizeof(int), 1, fout);
                DelBlock(Blocation3);
                file_inode->i_blocks--;
            }
            DelBlock(Blocation2);
        }
        DelBlock(block_location);
    }

    DelInode(file_inode->i_block[0]); // 删除文件对应的索引结点
    fclose(fout);
}

void RecursiveDeleteDirectory(int dir_inode_location) {
    FILE *fout = fopen(PATH, "r+");

    ext2_inode dir_inode;
    fseek(fout, 3 * blocksiz + dir_inode_location * sizeof(ext2_inode), SEEK_SET);
    fread(&dir_inode, sizeof(ext2_inode), 1, fout);

    // 遍历目录项
    for (int i = 0; i < 6; i++) {
        int block_location = dir_inode.i_block[i];
        if (block_location != 0) {
            int dir_entry_location = (data_begin_block + block_location) * blocksiz;
            ext2_dir_entry dir_entry;

            while (fread(&dir_entry, sizeof(ext2_dir_entry), 1, fout) == 1) {
                if (dir_entry.inode != 0) {
                    if (dir_entry.file_type == 2) {
                        RecursiveDeleteDirectory(dir_entry.inode);
                    } else if (dir_entry.file_type == 1) {
                        ext2_inode file_inode;
                        fseek(fout, 3 * blocksiz + dir_entry.inode * sizeof(ext2_inode), SEEK_SET);
                        fread(&file_inode, sizeof(ext2_inode), 1, fout);
                        DeleteFile(&file_inode);
                        DelInode(dir_entry.inode);
                    }

                    // 清空目录项
                    fseek(fout, dir_entry_location, SEEK_SET);
                    ext2_dir_entry empty_entry;
                    memset(&empty_entry, 0, sizeof(ext2_dir_entry));  // 使用 memset 将目录项清零
                    fwrite(&empty_entry, sizeof(ext2_dir_entry), 1, fout);
                }
                dir_entry_location += dirsiz;
            }
        }
    }

    fclose(fout);
    DelInode(dir_inode_location);
}

// 主函数修改如下
// 增加一个辅助函数，用于更新目录的信息
void UpdateCurrentDirectory(FILE *fout, ext2_inode *current) {
    ext2_dir_entry bentry;
    fseek(fout, (data_begin_block + current->i_block[0]) * blocksiz, SEEK_SET);
    fread(&bentry, sizeof(ext2_dir_entry), 1, fout); // 获取当前目录的目录项
    fseek(fout, 3 * blocksiz + (bentry.inode + 1) * sizeof(ext2_inode), SEEK_SET);
    fwrite(current, sizeof(ext2_inode), 1, fout); // 将当前目录修改的信息写回文件
}

int Delet(int type, ext2_inode *current, char *name) {
    FILE *fout = fopen(PATH, "r+");
    int i, j, t, flag;
    int node_location, dir_entry_location, block_location;
    ext2_inode cinode;
    ext2_dir_entry bentry, centry, dentry;
    char var;

    // 初始化一个空的目录项
    dentry.inode = 0;
    dentry.rec_len = sizeof(ext2_dir_entry);
    dentry.name_len = 0;
    dentry.file_type = 0;
    strcpy(dentry.name, "");
    dentry.dir_pad = 0;

    t = (int)(current->i_size / dirsiz); // 总条目数
    flag = 0;                            // 是否找到文件或目录

    // 查找目标文件或目录的位置
    for (i = 0; i < t; i++) {
        dir_entry_location = dir_entry_position(i * dirsiz, current->i_block);
        fseek(fout, dir_entry_location, SEEK_SET);
        fread(&centry, sizeof(ext2_dir_entry), 1, fout);
        if ((strcmp(centry.name, name) == 0) && (centry.file_type == type)) {
            flag = 1;
            j = i;
            break;
        }
    }

    if (flag) {
        node_location = centry.inode;
        fseek(fout, 3 * blocksiz + node_location * sizeof(ext2_inode), SEEK_SET); // 定位INODE位置
        fread(&cinode, sizeof(ext2_inode), 1, fout);
        block_location = cinode.i_block[0];

        // 删除文件夹
        if (type == 2) {
            // if (cinode.i_size > 2 * dirsiz) {
            //     printf("The folder is not empty! Are you sure to delete it?\n");
            //     printf("[Y/N]");
            //     scanf("%c", &var);
            //     scanf("%c", &var);
            //     if (var == 'N' || var == 'n')
            //         return 1;
            //     else if (var == 'Y' || var == 'y')
            //         printf("deleting......\n");
            //     else
            //         printf("wrong input! delete failure.\n");
            // }
            RecursiveDeleteDirectory(node_location);

            // 找到 current 指向的目录的最后一个目录项
            dir_entry_location = dir_entry_position(current->i_size - dirsiz, current->i_block);
            fseek(fout, dir_entry_location, SEEK_SET);
            fread(&centry, sizeof(ext2_dir_entry), 1, fout); // 将最后一个目录项保存在 centry 中
            fseek(fout, dir_entry_location, SEEK_SET);
            fwrite(&dentry, sizeof(ext2_dir_entry), 1, fout);                  // 清空该位置
            dir_entry_location -= data_begin_block * blocksiz;                // 在数据中的位置

            // 如果这个位置刚好是一个块的起始位置，则删掉这个块
            if (dir_entry_location % blocksiz == 0) {
                DelBlock((int)(dir_entry_location / blocksiz));
                current->i_blocks--;

                if (current->i_blocks == 6)
                    DelBlock(current->i_block[6]);
                else if (current->i_blocks == (blocksiz / sizeof(int) + 6)) {
                    int a;
                    fseek(fout, data_begin_block * blocksiz + current->i_block[7] * blocksiz, SEEK_SET);
                    fread(&a, sizeof(int), 1, fout);
                    DelBlock(a);
                    DelBlock(current->i_block[7]);
                } else if (!((current->i_blocks - 6 - blocksiz / sizeof(int)) % (blocksiz / sizeof(int)))) {
                    int a;
                    fseek(fout, data_begin_block * blocksiz + current->i_block[7] * blocksiz + ((current->i_blocks - 6 - blocksiz / sizeof(int)) / (blocksiz / sizeof(int))), SEEK_SET);
                    fread(&a, sizeof(int), 1, fout);
                    DelBlock(a);
                }
            }
            current->i_size -= dirsiz;

            if (j * dirsiz < current->i_size) { // 删除的不是最后一个目录项，用 centry 覆盖
                dir_entry_location = dir_entry_position(j * dirsiz, current->i_block);
                fseek(fout, dir_entry_location, SEEK_SET);
                fwrite(&centry, sizeof(ext2_dir_entry), 1, fout);
            }

            //更新当前目录的信息
            UpdateCurrentDirectory(fout, current);

            printf("The %s is deleted!\n", name);
        }
        // 删除文件
        else if (type == 1) {
            DeleteFile(&cinode);

            dir_entry_location = dir_entry_position(current->i_size - dirsiz, current->i_block); // 找到 current 指向条目的最后一条

            fseek(fout, dir_entry_location, SEEK_SET);
            fread(&centry, sizeof(ext2_dir_entry), 1, fout); // 将最后一条条目存入 centry
            fseek(fout, dir_entry_location, SEEK_SET);
            fwrite(&dentry, sizeof(ext2_dir_entry), 1, fout);                  // 清空该位置
            dir_entry_location -= data_begin_block * blocksiz;                // 在数据中的位置

            // 如果这个位置刚好是一个块的起始位置，则删掉这个块
            if (dir_entry_location % blocksiz == 0) {
                DelBlock((int)(dir_entry_location / blocksiz));
                current->i_blocks--;

                if (current->i_blocks == 6)
                    DelBlock(current->i_block[6]);
                else if (current->i_blocks == (blocksiz / sizeof(int) + 6)) {
                    int a;
                    fseek(fout, data_begin_block * blocksiz + current->i_block[7] * blocksiz, SEEK_SET);
                    fread(&a, sizeof(int), 1, fout);
                    DelBlock(a);
                    DelBlock(current->i_block[7]);
                } else if (!((current->i_blocks - 6 - blocksiz / sizeof(int)) % (blocksiz / sizeof(int)))) {
                    int a;
                    fseek(fout, data_begin_block * blocksiz + current->i_block[7] * blocksiz + ((current->i_blocks - 6 - blocksiz / sizeof(int)) / (blocksiz / sizeof(int))), SEEK_SET);
                    fread(&a, sizeof(int), 1, fout);
                    DelBlock(a);
                }
            }
            current->i_size -= dirsiz;

            if (j * dirsiz < current->i_size) { // 删除的条目如果不是最后一条，用 centry 覆盖
                dir_entry_location = dir_entry_position(j * dirsiz, current->i_block);
                fseek(fout, dir_entry_location, SEEK_SET);
                fwrite(&centry, sizeof(ext2_dir_entry), 1, fout);
            }

            // 更新当前目录的信息
            UpdateCurrentDirectory(fout, current);

            printf("The %s is deleted!\n", name);
        }
    } else {
        fclose(fout);
        return 1; // 未找到目标文件或目录
    }
    fclose(fout);
    return 0;
}

/*main shell*/
void shellloop(ext2_inode currentdir) {
    char command[10], var1[10], var2[10] = " " , var3[128], path[10];
    ext2_inode temp;
    int i, j, t;
    char currentstring[20];
    char ctable[12][10] = { "create", "delete", "cd", "close", "read", "write", "password", "format","exit","login","logout","ls"}; 

    while (1) {
        // 获取当前目录的目录名
        getstring(currentstring, currentdir);
        printf("\n%s=># ", currentstring);
        scanf("%s", command);

        // 查找用户输入的命令在命令表中的位置
        for (i = 0; i < 12; i++)
            if (!strcmp(command, ctable[i]))
                break;

        // 根据命令执行相应的操作
        if (i == 0 || i == 1) { // 创建、删除 文件/目录
            printf("input the type of file [f/d]: ");
            scanf("%s", var1); // 用来接收操作
            if(i == 0){
                if(var1[0] == 'f')
                {
                    printf("input the mode of file [r/w/rw]: ");
                    fflush(stdin);
                    scanf("%s", var2); // 用来接收权限
                }
                else strcpy(var2,"rw"); // 文件的格式
            }
            fflush(stdin);
            scanf("%s", var3); // 用来接收名字

            if (var1[0] == 'f')
                j = 1; // 创建文件
            else if (var1[0] == 'd')
                j = 2; // 创建目录
            else {
                printf("the first variant must be [f/d]");
                continue;
            }

            if(i == 0){
                if (!strcmp(var2,"r"))
                    t = 1; // 可读文件             
                else if (!strcmp(var2,"w"))
                    t = 2; // 可写目录
                else if (!strcmp(var2,"rw"))
                    t = 3; // 可读写目录
                else {
                    printf("the second variant must be [r/w/rw]");
                    continue;
                }
            }

            if (i == 0) {
                if (Create(j, t, &currentdir, var3) == 1)
                    printf("Failed! %s can't be created\n", var3);
                else
                    printf("Congratulations! %s is created\n", var3);
            } else {
                if (Delet(j, &currentdir, var3) == 1)
                    printf("Failed! %s can't be deleted!\n", var3);
                else
                    printf("Congratulations! %s is deleted!\n", var3);
            }
        } 
        else if (i == 2) { // cd 切换目录
            scanf("%s", var2); // 接收目的目录名
            i = 0;
            j = 0;
            temp = currentdir;
            while (1) {
                path[i] = var2[j];
                if (path[i] == '/') {
                    if (j == 0)
                        initialize(&currentdir);
                    else if (i == 0) {
                        printf("path input error!\n");
                        break;
                    } 
                    else {
                        path[i] = '\0';
                        if (Open(&currentdir, path) == 1) {
                            printf("path input error!\n");
                            currentdir = temp;
                        }
                    }
                    i = 0;
                } 
                else if (path[i] == '\0') {
                    if (i == 0)
                        break;
                    if (Open(&currentdir, path) == 1) {
                        printf("path input error!\n");
                        currentdir = temp;
                    }
                    break;
                } 
                else
                    i++;
                j++;
            }
        } 
        else if (i == 3) { // close
            printf("the number of layers to get out of:");
            scanf("%d", &i); // 要退出的层数
            for (j = 0; j < i; j++)
                if (Close(&currentdir) == 1) {
                    printf("Warning! the number %d is too large\n", i);
                    break;
                }
        } 
        else if (i == 4) { // read
            scanf("%s", var2);
            if (Read(&currentdir, t, var2) == 1)
                printf("Failed! The file can't be read\n");
        } 
        else if (i == 5) { // write
            scanf("%s", var2);
            if (Write(&currentdir, t, var2) == 1)
                printf("Failed! The file can't be written\n");
        } 
        else if (i == 6) // password
            Password();
        else if (i == 7) { // format
            while (1) {
                printf("Do you want to format the filesystem?\n It will be dangerous to your data.\n"); 
                printf("[Y/N]"); 
                scanf("%s",var1); 
                if(var1[0]=='N'||var1[0]=='n') 
                    break; 
                else if(var1[0]=='Y'|| var1[0]=='y') {
                    format();
                    break; 
                } else 
                    printf("please input [Y/N]");
            }
        } else if (i == 8) { // exit
            while (1) {
                printf("Do you want to exit from filesystem?[Y/N]");
                scanf("%s", &var2);
                if (var2[0] == 'N' || var2[0] == 'n')
                    break;
                else if (var2[0] == 'Y' || var2[0] == 'y')
                    return;
                else
                    printf("please input [Y/N]");
            }
        } 
        else if (i == 9) // login
            printf("Failed! You haven't logged out yet\n");
        else if (i == 10) { // logout
            while (i) {
                printf("Do you want to logout from filesystem?[Y/N]");
                scanf("%s", var1);
                if (var1[0] == 'N' || var1[0] == 'n')
                    break;
                else if (var1[0] == 'Y' || var1[0] == 'y') {
                    initialize(&currentdir);
                    while (1) {
                        printf("$$$$=># ");
                        scanf("%s", var2);
                        if (strcmp(var2, "login") == 0) {
                            if (login() == 0) {
                                i = 0;
                                break;
                            }
                        } else if (strcmp(var2, "exit") == 0)
                            return;
                    }
                } else
                    printf("please input [Y/N]");
            }
        } else if (i == 11) // ls
            Ls(&currentdir);
        else
            printf("Failed! Command not available\n");
    }
}

int main()
{
    ext2_inode cu; /*current user*/
    printf("Hello! Welcome to Ext2_like file system!\n");
    if (initfs(&cu) == 1)
        return 0;
    /*******登陆********/
    if (login() != 0) 
    {
        printf("Wrong password!It will terminate right away.\n");
        exitdisplay();
        return 0;
    }
    shellloop(cu);
    exitdisplay();
    return 0;
}
