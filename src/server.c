#include <stddef.h>
#include <stdio.h>
#include <stddefer.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "net_utils.h"

// OLD VERSION; USE CHAT_SERVER.C INSTEAD

#define SERVER_STR_PATTERN "Server: %.*s"

typedef struct{
    char buff[1024]; 
    size_t used_bytes; 
    int fd; 
} client_data; 

typedef enum{
    CLIENT_ERROR = -1,
    CLIENT_CONTINUE = 0, 
    CLIENT_DONE = 1, 
} client_result;

static client_result handle_client(client_data* client); 

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
        .sin_addr = {htonl(INADDR_LOOPBACK)}
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
    
    server_loop: while(true){
        struct sockaddr_in client_socket_addr = {0};
        socklen_t client_socket_len = sizeof(client_socket_addr);
        int client_fd = accept(socket_fd, (struct sockaddr*) &client_socket_addr, &client_socket_len); 
        if(client_fd == -1){
            perror("accept"); 
            continue; 
        } else {
            const struct in_addr* client_address = &client_socket_addr.sin_addr;
            char client_address_buffer[INET_ADDRSTRLEN]; 
            inet_ntop(AF_INET,client_address , client_address_buffer, INET_ADDRSTRLEN); 
            const in_port_t client_port = ntohs(client_socket_addr.sin_port);
            
            printf("Client: %s:%d\n", client_address_buffer, client_port);
        } 
        defer close(client_fd); 

        int fork_ret = fork();
        
        switch (fork_ret){
            case -1: 
                perror("fork"); 
                break server_loop;
            case 0:{
                close(socket_fd); 
                client_data client = {
                    .buff = {0}, 
                    .fd = client_fd, 
                    .used_bytes = 0
                }; 

                client_loop: while(true){
                    client_result handle_result = handle_client(&client);
                    switch (handle_result){  
                        case CLIENT_CONTINUE: 
                            continue client_loop; 
                        case CLIENT_ERROR:
                        case CLIENT_DONE: 
                            break client_loop; 
                    }
                } 
                exit(EXIT_SUCCESS);  
            }
            default: 
                continue;
        }
    }
    return EXIT_SUCCESS; 
}

static client_result handle_client(client_data* client){
    if (client->used_bytes >= (int) sizeof(client->buff)) {
        fprintf(stderr, "Line too long!\n"); 
        return CLIENT_ERROR;
    }

    int bytes_received = recv(client->fd, client->buff + client->used_bytes, sizeof(client->buff) - client->used_bytes, 0);
    if(bytes_received == 0) return CLIENT_DONE;  
    if(bytes_received == -1){
        perror("recv"); 
        return CLIENT_ERROR; 
    }
    client->used_bytes += bytes_received; 

    char helper_b[sizeof(client->buff) + sizeof(SERVER_STR_PATTERN)]; 
    char* nl; 
    int len; 
    while((nl = memchr(client->buff, '\n', client->used_bytes)) != NULL){
        int line_len = nl - client->buff + 1; 

        len = snprintf(helper_b, sizeof(helper_b),SERVER_STR_PATTERN, line_len, client->buff);
        if(len >= (int) sizeof(helper_b)){
            fprintf(stderr, "String too long!\n");
            return CLIENT_ERROR; 
        }
        if(len < 0){
            fprintf(stderr, "Encoding error!\n"); 
            return CLIENT_ERROR;
        }
        memmove(client->buff, client->buff + line_len, client->used_bytes - line_len); 
        client->used_bytes -= line_len; 
        
        printf("%.*s", len, helper_b); 
        int ret_s = send_all(client->fd, helper_b, (size_t) len); 
        if(ret_s == -1){
            perror("send_all"); 
            return CLIENT_ERROR; 
        }
    } 
    return CLIENT_CONTINUE; 
}