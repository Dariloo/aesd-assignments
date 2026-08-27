#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>

volatile sig_atomic_t stop = 0;

void signal_handler(int signo)
{
    stop = 1;
}

int main(int argc, char *argv[])
{
    int sockfd;
    int client_fd;
    char buffer[1024];
    ssize_t bytes_received;

    char *all_data = NULL;
    size_t all_data_size = 0;

    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len;

    if (argc == 2 && strcmp(argv[1], "-d") == 0)
    {
        if (daemon(0, 0) == -1)
        {
            perror("daemon");
            return 1;
        }
    }

    struct sigaction action = {0};

    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        close(sockfd);
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, 10) == -1)
    {
        perror("listen");
        close(sockfd);
        return 1;
    }

    while (!stop)
    {
        char *message = NULL;
        size_t message_size = 0;

        client_addr_len = sizeof(client_addr);

        client_fd = accept(sockfd,
                           (struct sockaddr *)&client_addr,
                           &client_addr_len);

        if (client_fd == -1)
        {
            if (errno == EINTR && stop)
            {
                break;
            }

            perror("accept");
            break;
        }

        while (1)
        {
            bytes_received = recv(client_fd,
                                  buffer,
                                  sizeof(buffer),
                                  0);

            if (bytes_received < 0)
            {
                perror("recv");
                break;
            }

            if (bytes_received == 0)
            {
                break;
            }

            char *temp;

            temp = realloc(message,
                           message_size + bytes_received + 1);

            if (temp == NULL)
            {
                free(message);
                message = NULL;
                break;
            }

            message = temp;

            memcpy(message + message_size,
                   buffer,
                   bytes_received);

            message_size += bytes_received;
            message[message_size] = '\0';

            if (strchr(message, '\n') != NULL)
            {
                break;
            }
        }

        if (message != NULL)
        {
            char *temp;

            temp = realloc(all_data,
                           all_data_size + message_size);

            if (temp != NULL)
            {
                all_data = temp;

                memcpy(all_data + all_data_size,
                       message,
                       message_size);

                all_data_size += message_size;

                size_t total_sent = 0;

                while (total_sent < all_data_size)
                {
                    ssize_t bytes_sent;

                    bytes_sent = send(client_fd,
                                      all_data + total_sent,
                                      all_data_size - total_sent,
                                      0);

                    if (bytes_sent <= 0)
                    {
                        break;
                    }

                    total_sent += bytes_sent;
                }
            }
        }

        free(message);
        close(client_fd);
    }

    free(all_data);
    close(sockfd);

    return 0;
}
