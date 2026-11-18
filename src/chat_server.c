#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stddefer.h>
#include <poll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "net_utils.h"
#include "protocol.h"
#include "server_messages.h"

#define MAX_CLIENTS 64

typedef struct{
    char buff[MAX_LINE_LEN]; 
    size_t used_bytes; 
    int fd; 
    char name[MAX_NAME_LEN + 1];
} client_data; 

typedef enum{
    CLIENT_ERROR = -1,
    CLIENT_CONTINUE = 0, 
    CLIENT_DONE = 1, 
} client_result;

static client_result handle_client(client_data* c); 

typedef enum{
    PFD_LISTENER = 0,
    PFD_FIRST_CLIENT = 1, 
    PFD_COUNT = MAX_CLIENTS + 1
} FD_type;

static struct pollfd pfds[PFD_COUNT] = {
    [PFD_FIRST_CLIENT ... PFD_COUNT -1] = {.fd = -1}
}; 
static client_data clients[MAX_CLIENTS] = {
    [0 ... MAX_CLIENTS - 1] = {.fd = -1}
}; 

static void broadcast(const char* line, size_t len, int except); 
static void free_slot(int idx); 
static bool is_named(const client_data* c); 
static bool name_taken(const char* name, size_t len); 
static void send_error(int fd, chat_error e); 

int main(){
    int socket_fd = socket(AF_INET,SOCK_STREAM, 0);
    if(socket_fd == -1){
        perror("socket");
        return EXIT_FAILURE; 
    }
    defer close(socket_fd);  

    int socket_on = 1;
    int ret_set = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &socket_on, sizeof(socket_on));
    if (ret_set == -1){
        perror("setsockopt");
        return EXIT_FAILURE; 
    } 

    const struct sockaddr_in socket_address = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
        .sin_addr = {htonl(INADDR_ANY)}
    };

    int bind_ret = bind(socket_fd, (const struct sockaddr*) &socket_address, sizeof(socket_address)); 
    if(bind_ret == -1){
        perror("bind");
        return EXIT_FAILURE; 
    }

    if(listen(socket_fd, 5) == -1){
        perror("listen"); 
        return EXIT_FAILURE; 
    }

    pfds[PFD_LISTENER] = (struct pollfd) {
        .fd = socket_fd,
        .events = POLLIN
    }; 

    chat_server_loop: while (1) {
        if(poll(pfds, PFD_COUNT, -1) == -1){
            if(errno == EINTR) continue chat_server_loop;  
            else {
                perror("poll"); 
                return EXIT_FAILURE; 
            }  
        }

        // Listener 
        if(pfds[PFD_LISTENER].revents & POLLIN){
            struct sockaddr_in client_socket_addr = {0};
            socklen_t client_socket_len = sizeof(client_socket_addr);
            int client_fd = accept(
                socket_fd,
                (struct sockaddr*) &client_socket_addr,
                &client_socket_len
            );

            if(client_fd == -1){
                perror("accept");
            } else {
                int client_idx = PFD_FIRST_CLIENT; 
                while(client_idx < PFD_COUNT && pfds[client_idx].fd != -1) client_idx ++;
                if(client_idx == PFD_COUNT) {
                    send_error(client_fd, ERR_SERVER_FULL); 
                    close(client_fd); 
                } else {
                    clients[client_idx - PFD_FIRST_CLIENT] = (client_data) {.fd = client_fd};
                    pfds[client_idx] = (struct pollfd) {.fd = client_fd, .events = POLLIN}; 
                    printf("Client connected to slot %d\n", client_idx - PFD_FIRST_CLIENT); 
                }
            }
        }

        // Client
        client_loop: for(int idx = 0; idx < MAX_CLIENTS; idx++){
            struct pollfd *p = &pfds[idx + PFD_FIRST_CLIENT];
            if(p->fd == -1) continue client_loop; 
            if(p->revents & (POLLIN | POLLHUP | POLLERR)){
                client_data* c = &clients[idx]; 
                if(handle_client(c) != CLIENT_CONTINUE){
                    free_slot(idx); 
                }
            }
        }
    }   
    return EXIT_SUCCESS; 
}

static client_result handle_client(client_data* c){
    if(c->used_bytes >= sizeof(c->buff)){
        fprintf(stderr, "Line too long!\n"); 
        return CLIENT_ERROR; 
    }
    
    ssize_t r = recv(c->fd, c->buff + c->used_bytes, sizeof(c->buff) - c->used_bytes, 0); 
    if(r == 0) return CLIENT_DONE; 
    if(r == -1) {
        perror("recv");
        return CLIENT_ERROR; 
    }
    c->used_bytes += r; 

    char* nl; 
    while((nl = memchr(c->buff, '\n', c->used_bytes)) != NULL){
        int line_len = nl - c->buff + 1;
        line_result res = parse_line(c->buff, line_len); 
        switch (res.command) {
            case CHAT_NICK: {
                if(is_named(c)) 
                    send_error(c->fd, ERR_ALREADY_NAMED);
                else if(!is_name_valid(res.argument, res.argument_len))
                    send_error(c->fd, ERR_INVALID_NAME);
                else if(name_taken(res.argument, res.argument_len)) 
                    send_error(c->fd, ERR_NAME_TAKEN);
                else {
                    memcpy(c->name, res.argument, res.argument_len); 
                    c->name[res.argument_len] = '\0'; 
                    (void) send_all(c->fd, server_template(CHAT_OK), strlen(server_template(CHAT_OK))); 
                    char buff[MAX_LINE_LEN]; 
                    int ret_size = snprintf(buff, sizeof(buff), server_template(CHAT_JOIN), c->name); 
                    int client_idx = c - clients; 
                    broadcast(buff, ret_size, client_idx); 
                }
                break; 
            }
            case CHAT_MSG: {
                char message_buff[MAX_LINE_LEN]; 
                int message_len = snprintf(message_buff, sizeof(message_buff), server_template(CHAT_MSG), c->name, (int) res.argument_len, res.argument); 
                if(message_len < 0 || message_len >= (int) sizeof(message_buff)){
                    send_error(c->fd, ERR_MESSAGE_TOO_LONG);
                    break; 
                }
                broadcast(message_buff, message_len, -1); 
                break; 
            }
            case CHAT_QUIT: 
                return CLIENT_DONE; 
            default:
                send_error(c->fd, ERR_UNKNOWN_COMMAND); 
                break; 
        }
        memmove(c->buff, c->buff + line_len, c->used_bytes - line_len); 
        c->used_bytes -= line_len; 
    }

    return CLIENT_CONTINUE;
}

static void broadcast(const char* line, size_t len, int except){
    client_iter: for(int idx = 0; idx < MAX_CLIENTS; idx++){
        if( clients[idx].fd == -1 || 
            !is_named(&clients[idx]) ||
            idx == except
        ) continue client_iter;

        if(send_all(clients[idx].fd, line, len) == -1){
            fprintf(stderr, "Couldn't send to client in slot: %d\n", idx); 
        } 
    }
}

static void free_slot(int idx){
    client_data *c = &clients[idx];
    char buff[MAX_LINE_LEN]; 
    if(is_named(c)){
        int snp_ret = snprintf(buff, sizeof(buff), server_template(CHAT_LEAVE), c->name); 
        broadcast(buff, snp_ret, idx);
    }
    close(c->fd); 
    clients[idx] = (client_data){.fd = -1};
    pfds[idx + PFD_FIRST_CLIENT].fd = -1; 
    printf("Client left slot: %d\n", idx);
}

static bool is_named(const client_data* c){
    return c->name[0] != '\0'; 
} 

static bool name_taken(const char* name, size_t len){
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(strlen(clients[i].name) == len && memcmp(clients[i].name, name, len) == 0) return true; 
    }
    return false; 
}

static void send_error(int fd, chat_error e){
    char err_buff[MAX_LINE_LEN];
    const char *reason = error_reason(e);  
    if (reason == NULL) return;
    int err_size = snprintf(err_buff, sizeof(err_buff), server_template(CHAT_ERR), reason);
    if(err_size < 0 || err_size >= (int) sizeof(err_buff)) return; 
    (void) send_all(fd, err_buff, err_size);         
}