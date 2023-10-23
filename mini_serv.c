#include <unistd.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/select.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>

typedef struct s_client {
    int id;
    char    msg[1024];
} t_client;

t_client clients[1024];
char read_buf[120000], write_buf[120000];
fd_set all_sockets_set, read_set, write_set;
int next_client_id, socket_fd_max = 0;

void exit_with_error(char *error_msg){
    write(2, error_msg, strlen(error_msg));
    exit(1);
}

void send_msg_to_all(int sender_socket){
    for (int client_socket = 0; client_socket <= socket_fd_max; client_socket++){
        if (client_socket != sender_socket && FD_ISSET(client_socket, &write_set)){
            write(client_socket, write_buf, strlen(write_buf));
            printf("sent [%s] to socket %d.\n", write_buf, client_socket);
        }
    }
}

int main(int ac, char **av){
    if (ac != 2)
        exit_with_error("Wrong number of arguments\n");
    
    struct  sockaddr_in listen_socket_addr;
    socklen_t len_dump;
    
    bzero(&clients, sizeof(clients));
    bzero(&listen_socket_addr, sizeof(listen_socket_addr));

    listen_socket_addr.sin_family = AF_INET;
    listen_socket_addr.sin_port = htons(atoi(av[1]));
    listen_socket_addr.sin_addr.s_addr = htonl(127 << 24 | 1);

    int listen_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    int dump = 1;
    if (listen_socket_fd < 0
        || setsockopt(listen_socket_fd, SOL_SOCKET, SO_REUSEADDR, &dump, sizeof(int)) == -1
        || bind(listen_socket_fd, (struct  sockaddr *)&listen_socket_addr, sizeof(listen_socket_addr)) < 0
        || listen(listen_socket_fd, 10) < 0)
        exit_with_error("Fatal erorr\n");

    FD_SET(listen_socket_fd, &all_sockets_set);
    socket_fd_max = listen_socket_fd;

    while (42) {
        write_set = read_set = all_sockets_set;
        if (select(socket_fd_max + 1, &read_set, &write_set, NULL, NULL) < 0)
            continue;
        for (int socket_fd = 0; socket_fd <= socket_fd_max; socket_fd++) {
            if (FD_ISSET(socket_fd, &read_set) && socket_fd == listen_socket_fd) {
                int new_connexion = accept(listen_socket_fd, (struct  sockaddr *)&listen_socket_addr, &len_dump);
                if (new_connexion < 0)
                    break;
                // printf("New connexion: %d\n", new_connexion);
                FD_SET(new_connexion, &all_sockets_set);
                clients[new_connexion].id = next_client_id++;
                socket_fd_max = socket_fd_max < new_connexion ?  new_connexion : socket_fd_max;
                sprintf(write_buf, "server: client %d just arrived\n", clients[new_connexion].id);
                send_msg_to_all(new_connexion);
                break;
            } 
            if (FD_ISSET(socket_fd, &read_set) && socket_fd != listen_socket_fd) {
                ssize_t received_bytes = read(socket_fd, read_buf, 120000);
                if (received_bytes <= 0) {
                    // printf("client left\n");
                    sprintf(write_buf, "server: client %d just left\n", clients[socket_fd].id);
                    send_msg_to_all(socket_fd);
                    FD_CLR(socket_fd, &all_sockets_set);
                    close(socket_fd);
                } else {
                    for (int i_client = 0, i_buf = 0; i_buf < received_bytes; i_buf++, i_client++) {
                        // printf("Writting '%c' to client msg buff at i %d.\n", read_buf[i_buf], i_client);
                        clients[socket_fd].msg[i_client] = read_buf[i_buf];
                        if (read_buf[i_buf] == '\n') {
                            clients[socket_fd].msg[i_client] = 0;
                            sprintf(write_buf, "client %d: %s\n", clients[socket_fd].id, clients[socket_fd].msg);
                            send_msg_to_all(socket_fd);
                            i_client = -1;
                        }
                    }
                }
                break;
            }
        }
    }
}