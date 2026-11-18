#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <stddefer.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "net_utils.h"
#include "protocol.h"

#define NICKNAME_PREFIX "NICK "
#define QUIT_LINE "QUIT\n"

enum session_state{
    SESSION_WAITING,
    SESSION_CHOOSING,
    SESSION_CHATTING
};

enum server_line_result {
    SERVER_LINE_OK,
    SERVER_LINE_ERROR
};

enum ARG_enum {
    PROGRAM,
    IP_ADDRESS,
    IP_PORT,
    NICKNAME
};

enum FD_enum {
    FD_STDIN,
    FD_SOCKET,

    FD_COUNT
};
static enum server_line_result handle_server_line(const char* in_buff, size_t line_len, enum session_state *state);

int main(int argc, char** argv){
    if(argc != 4) {
        fprintf(stderr, "Incorrect use of command!\nCorrect Usage: ./client $IP $PORT $NICK\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_address = {
        .sin_port = htons(atoi(argv[IP_PORT])),
        .sin_family = AF_INET
    };

    int inet_ret = inet_pton(AF_INET, argv[IP_ADDRESS], &(server_address.sin_addr));
    switch (inet_ret){
        case -1:
            perror("inet_pton");
            return EXIT_FAILURE;
        case 0:
            fprintf(stderr, "Bad IP Address!\nExpected -> [0-255].[0-255].[0-255].[0-255]\nGot-> %s\n", argv[IP_ADDRESS]);
            return EXIT_FAILURE;
        case 1:
            break;
    }

    int socket_fd = socket(AF_INET,SOCK_STREAM, 0);
    if(socket_fd == -1){
        perror("socket");
        return EXIT_FAILURE;
    }
    defer close(socket_fd);

    if(!is_name_valid(argv[NICKNAME], strlen(argv[NICKNAME]))) {
        fprintf(stderr, "Invalid Name!\n Valid characters: letters, '-', '_',\n Max size: 32\n");
        return EXIT_FAILURE;
    }
    enum session_state session = SESSION_WAITING;

    int con_ret = connect(socket_fd, (const struct sockaddr*) &server_address, sizeof(server_address));
    if(con_ret == -1){
        perror("connect");
        return EXIT_FAILURE;
    }


    char out_buff[MAX_LINE_LEN];
    char line_buff[MAX_LINE_LEN];
    char in_buff[MAX_LINE_LEN];

    int len = snprintf(out_buff, sizeof(out_buff),"%s%s\n", NICKNAME_PREFIX, argv[NICKNAME]);
    int send_all_ret = send_all(socket_fd, out_buff, len);
    if(send_all_ret == -1){
        perror("send_all");
        return EXIT_FAILURE;
    }

    size_t used = 0;
    const char* nl;

    struct pollfd io_list[FD_COUNT] = {
        [FD_SOCKET] = {.fd = socket_fd, .events = POLLIN},
        [FD_STDIN]  = {.fd = STDIN_FILENO, .events = POLLIN}
    };

    chat_loop: while(1){
        int n = poll(io_list, FD_COUNT, -1);
        if(n == -1){
            if (errno != EINTR){
                perror("poll");
                return EXIT_FAILURE;
            } else {
                continue chat_loop;
            }
        }

        // Network
        if(io_list[FD_SOCKET].revents & (POLLIN | POLLHUP | POLLERR)){
            if(used >= sizeof(in_buff)){
                fprintf(stderr, "Line too long!\n");
                return EXIT_FAILURE;
            }

            ssize_t r = recv(socket_fd, in_buff + used, sizeof(in_buff) - used, 0);
            if (r == 0) {
                fprintf(stderr, "Server Disconnected!\n");
                return EXIT_FAILURE;
            }
            if (r == -1){
                perror("recv");
                return EXIT_FAILURE;
            }

            used += r;
            while((nl = memchr(in_buff, '\n', used)) != NULL){
                size_t line_len = nl - in_buff + 1;
                enum server_line_result res = handle_server_line(in_buff, line_len, &session);
                switch (res){
                    case SERVER_LINE_ERROR:
                        fprintf(stderr, "Malformed command\nGot: %.*s",(int) line_len,  in_buff);
                        return EXIT_FAILURE;
                    case SERVER_LINE_OK:
                        break;
                }
                memmove(in_buff, in_buff + line_len, used - line_len);
                used -= line_len;
            }
        }

        // Keyboard
        if(io_list[FD_STDIN].revents & POLLIN){
            if(fgets(line_buff, sizeof(line_buff), stdin) == NULL) {
                (void) send_all(socket_fd, QUIT_LINE, sizeof(QUIT_LINE) - 1);
                break chat_loop;
            }
            switch(session){
                case SESSION_WAITING:
                    printf("Waiting on server!\n");
                    break;
                case SESSION_CHOOSING:{
                    size_t buf_len = strlen(line_buff);
                    if(buf_len > 0 && line_buff[buf_len - 1] == '\n'){
                        line_buff[buf_len - 1] = '\0';
                        buf_len--;
                    }
                    if(is_name_valid(line_buff, buf_len)){
                        len = snprintf(out_buff, sizeof(out_buff),"%s%s\n", NICKNAME_PREFIX, line_buff);
                        send_all_ret = send_all(socket_fd, out_buff, len);
                        if(send_all_ret == -1){
                            perror("send_all");
                            return EXIT_FAILURE;
                        }
                        session = SESSION_WAITING;
                    } else {
                        printf("Invalid Name, try again!\n");
                    }
                    break;
                }
                case SESSION_CHATTING:{
                    len = snprintf(out_buff, sizeof(out_buff), "MSG %s", line_buff);
                    if(len < 0 || len>=(int) sizeof(out_buff)) {
                        fprintf(stderr, "Truncated line!\n");
                        break; 
                    }
                    send_all_ret = send_all(socket_fd, out_buff, len);
                    if(send_all_ret == -1){
                        perror("send_all");
                        return EXIT_FAILURE;
                    }
                    break;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}


static enum server_line_result handle_server_line(const char* in_buff, size_t line_len, enum session_state *state){
    line_result res = parse_line(in_buff, line_len);
    switch (res.command) {
        case CHAT_MSG: {
            const char* space = memchr(res.argument, ' ', res.argument_len);
            if(space == NULL) {
                return SERVER_LINE_ERROR;
            }
            size_t name_len = space - res.argument;
            size_t text_len = res.argument_len - name_len - 1;
            printf("%.*s: %.*s\n", (int) name_len, res.argument, (int) text_len, space + 1);
            break;
        }
        case CHAT_JOIN:
            printf("* %.*s joined\n",(int) res.argument_len, res.argument);
            break;
        case CHAT_LEAVE:
            printf("* %.*s left\n",(int) res.argument_len, res.argument);
            break;
        case CHAT_ERR:
            fprintf(stderr, "error: %.*s\n",(int) res.argument_len, res.argument);
            if(*state != SESSION_CHATTING){
                printf("Choose another name!\n");
               *state = SESSION_CHOOSING;
            }
            break;
        case CHAT_OK:
            printf("* you're in\n");
            *state = SESSION_CHATTING;
            break;
        default:
    }
    return SERVER_LINE_OK;
}
