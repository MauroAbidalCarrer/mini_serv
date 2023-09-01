# include <stdio.h>
# include <stdlib.h>
# include <errno.h>
# include <unistd.h>
# include <string.h>
# include <netdb.h>
# include <sys/socket.h>
# include <sys/select.h>
# include <netinet/in.h>

# define FATAL_ERROR_MSG "Fatal error\n"
# define WRONG_NB_ARG_MSG "Wrong number of arguments\n"
# define MSG_BUFFER_SIZE 1200
// SEND_BUFFER_SIZE = MSG_BUFFER_SIZE + strlen("client 1024: ")
# define SEND_BUFFER_SIZE MSG_BUFFER_SIZE + 13

int socket_fd_max = 0;
int nex_client_id = 0;
int listen_socket = -1;
//Array of client ids, the index corresponds to the socket fd.
//Since select can only monitor fds that are lower FD_SETSIZE.
//So we can declare an array of size FD_SETSIZE without using malloc and be guaranteed to be able to track the maximum number of clients.
int client_ids[FD_SETSIZE];

fd_set every_socket_set;
fd_set write_socket_set;
fd_set read_socket_set;

char send_buffer[SEND_BUFFER_SIZE];

void error_exit(char *error_msg)
{
    write(2, error_msg, strlen(error_msg));
    if (listen_socket != -1)
        close(listen_socket);
    for (int socket_fd = 0; socket_fd < FD_SETSIZE; socket_fd++)
        if (client_ids[socket_fd] != -1)
            close(socket_fd);
    exit(1);
}

void broadcast_message(int emitter_socket)
{
    for (int socket_fd = 0; socket_fd <= socket_fd_max; socket_fd++)
        if (socket_fd != emitter_socket && FD_ISSET(socket_fd, &write_socket_set) && emitter_socket != listen_socket && client_ids[emitter_socket] != -1)
            write(socket_fd, send_buffer, strlen(send_buffer));
}

int main(int ac, char **av)
{
    if (ac != 2)
        error_exit(WRONG_NB_ARG_MSG);

    //start
    socklen_t           len;
    struct  sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET; 
	servaddr.sin_port = htons(atoi(av[1]));
	servaddr.sin_addr.s_addr = htonl(2130706433);
    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    int dump = 1;
    if (listen_socket < 0
        || setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &dump, sizeof(int)) == -1
        || bind(listen_socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1
        || listen(listen_socket, 10) == -1)
        error_exit(FATAL_ERROR_MSG);
    FD_ZERO(&every_socket_set);
    FD_SET(listen_socket, &every_socket_set);
    socket_fd_max = listen_socket;
    memset(client_ids, -1, FD_SETSIZE * sizeof(int));

    //main loop
    while (42)
    {
        write_socket_set = every_socket_set;
        read_socket_set = every_socket_set;
        if (select(socket_fd_max + 1, &read_socket_set, &write_socket_set, NULL, NULL) < 0)
            continue;//try to continue even if there was an error.
        for (int socket_fd = 0; socket_fd <= socket_fd_max; socket_fd++)
        {
            if (FD_ISSET(socket_fd, &read_socket_set))
            {
                if (socket_fd == listen_socket)
                {//accept new client
                    int new_connexion = accept(socket_fd, (struct sockaddr *)&servaddr, &len);
                    if (new_connexion == -1)
                        continue ;//try to continue even if there was an error.
                    socket_fd_max = new_connexion > socket_fd_max ? new_connexion : socket_fd_max;
                    client_ids[new_connexion] = nex_client_id++;
                    FD_SET(new_connexion, &every_socket_set);
                    sprintf(send_buffer, "server: client %d just arrived\n", client_ids[new_connexion]);
                    broadcast_message(new_connexion);
                    break ;//Restart select loop to send messages to new client.
                }
                else
                {//read and broadcast msg
                    char read_msg_buffer[MSG_BUFFER_SIZE];
                    ssize_t nb_bytes_read = read(socket_fd, read_msg_buffer, MSG_BUFFER_SIZE - 1);
                    if (nb_bytes_read <= 0)
                    {//handle client disconnection
                        sprintf(send_buffer, "server: client %d just left\n", client_ids[socket_fd]);
                        broadcast_message(socket_fd);
                        FD_CLR(socket_fd, &every_socket_set);
                        close(socket_fd);
                        client_ids[socket_fd] = -1;
                        break;//Restart select loop to prevent messages from getting broadcasted to the deleted connexion.
                    }
                    else
                    {//handle client message
                        read_msg_buffer[nb_bytes_read] = 0;
                        char line_buffer[MSG_BUFFER_SIZE];
                        for (size_t i = 0, j = 0; i < strlen(read_msg_buffer); i++, j++)
                        {
                            line_buffer[j] = read_msg_buffer[i];
                            if (read_msg_buffer[i] == '\n' || read_msg_buffer[i] == 0)
                            {
                                line_buffer[j] = 0;
                                sprintf(send_buffer, "client %d: %s\n", client_ids[socket_fd], line_buffer);
                                broadcast_message(socket_fd);
                                j = -1;//set to -1 as it will get incremented at the end of the iteration of the loop.
                            }
                        }
                    }
                }
            }
        }
    }
}