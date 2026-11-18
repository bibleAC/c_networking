#include "net_utils.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stddef.h>
#include <asm-generic/errno-base.h>

int send_all(int fd,const  char* buff, size_t len){
    size_t sent = 0;
    while(sent <  len){
        size_t n = send(fd, buff + sent, len - sent, MSG_NOSIGNAL);
        if (n == (size_t) -1){
            if (errno == EINTR) continue;
            else return -1;
        }
        sent += n;
    }
    return 0;
}
