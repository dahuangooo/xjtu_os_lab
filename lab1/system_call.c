//
//  
//
//  Created by 杜昊阳 on 2023/10/15.
//

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
    pid_t pid = getpid();
    printf("system_call PID: %d\n",pid);
    return 0;
}
