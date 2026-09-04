#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/queue.h>
#include <time.h>
#include <fcntl.h>

#ifndef USE_AESD_CHAR_DEVICE
#define USE_AESD_CHAR_DEVICE 1
#endif

#if USE_AESD_CHAR_DEVICE
#define DATA_PATH "/dev/aesdchar"
#else
#define DATA_PATH "/var/tmp/aesdsocketdata"
#endif

struct client_thread
{
    pthread_t thread;
    int client_fd;
    bool complete;

    SLIST_ENTRY(client_thread) entries;
};

SLIST_HEAD(thread_list, client_thread);

#if !USE_AESD_CHAR_DEVICE
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

volatile sig_atomic_t stop = 0;

void signal_handler(int signo)
{
    stop = 1;
}

void *client_thread_func(void *arg)
{
    struct client_thread *data = (struct client_thread *)arg;

    char buffer[1024];
    char *message = NULL;
    size_t message_size = 0;
    ssize_t bytes_received;

    data->complete = false;

    while (1)
    {
        bytes_received = recv(data->client_fd,
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
#if USE_AESD_CHAR_DEVICE
        int file_fd = open(DATA_PATH, O_RDWR);

        if (file_fd != -1)
        {
            size_t total_written = 0;

            while (total_written < message_size)
            {
                ssize_t bytes_written;

                bytes_written = write(file_fd,
                                      message + total_written,
                                      message_size - total_written);

                if (bytes_written <= 0)
                {
                    perror("write");
                    break;
                }

                total_written += bytes_written;
            }

            if (total_written == message_size)
            {
                ssize_t bytes_read;

                while ((bytes_read = read(file_fd,
                                          buffer,
                                          sizeof(buffer))) > 0)
                {
                    size_t total_sent = 0;

                    while (total_sent < (size_t)bytes_read)
                    {
                        ssize_t bytes_sent;

                        bytes_sent = send(data->client_fd,
                                          buffer + total_sent,
                                          bytes_read - total_sent,
                                          0);

                        if (bytes_sent <= 0)
                        {
                            break;
                        }

                        total_sent += bytes_sent;
                    }
                }

                if (bytes_read < 0)
                {
                    perror("read");
                }
            }

            close(file_fd);
        }
        else
        {
            perror("open");
        }
#else
        pthread_mutex_lock(&file_mutex);

        FILE *file = fopen(DATA_PATH, "a+");

        if (file != NULL)
        {
            fwrite(message, 1, message_size, file);
            fflush(file);

            rewind(file);

            size_t bytes_read;

            while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
            {
                size_t total_sent = 0;

                while (total_sent < bytes_read)
                {
                    ssize_t bytes_sent;

                    bytes_sent = send(data->client_fd,
                                      buffer + total_sent,
                                      bytes_read - total_sent,
                                      0);

                    if (bytes_sent <= 0)
                    {
                        break;
                    }

                    total_sent += bytes_sent;
                }
            }

            fclose(file);
        }
        else
        {
            perror("fopen");
        }

        pthread_mutex_unlock(&file_mutex);
#endif
    }

    free(message);
    close(data->client_fd);

    data->complete = true;

    return NULL;
}

#if !USE_AESD_CHAR_DEVICE
void *timestamp_thread_func(void *arg)
{
    int i;

    while (!stop)
    {
        for (i = 0; i < 10 && !stop; i++)
        {
            sleep(1);
        }

        if (stop)
        {
            break;
        }

        time_t now;
        struct tm *time_info;
        char timestamp[128];

        now = time(NULL);
        time_info = localtime(&now);

        if (time_info == NULL)
        {
            continue;
        }

        strftime(timestamp,
                 sizeof(timestamp),
                 "timestamp:%a, %d %b %Y %H:%M:%S %z\n",
                 time_info);

        pthread_mutex_lock(&file_mutex);

        FILE *file = fopen(DATA_PATH, "a");

        if (file != NULL)
        {
            fputs(timestamp, file);
            fclose(file);
        }

        pthread_mutex_unlock(&file_mutex);
    }

    return NULL;
}
#endif

int main(int argc, char *argv[])
{
    int sockfd;
    int client_fd;

    struct sockaddr_in server_addr = {0};
    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len;

    struct thread_list threads;
    SLIST_INIT(&threads);

#if !USE_AESD_CHAR_DEVICE
    pthread_t timestamp_thread;
    bool timestamp_thread_started = false;
#endif

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

    if (setsockopt(sockfd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) == -1)
    {
        perror("setsockopt");
        close(sockfd);
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) == -1)
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

#if !USE_AESD_CHAR_DEVICE
    if (pthread_create(&timestamp_thread,
                       NULL,
                       timestamp_thread_func,
                       NULL) != 0)
    {
        perror("pthread_create");
        close(sockfd);
        return 1;
    }

    timestamp_thread_started = true;
#endif

    while (!stop)
    {
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

        struct client_thread *data;

        data = malloc(sizeof(struct client_thread));

        if (data == NULL)
        {
            close(client_fd);
            continue;
        }

        data->client_fd = client_fd;
        data->complete = false;

        if (pthread_create(&data->thread,
                           NULL,
                           client_thread_func,
                           data) != 0)
        {
            close(client_fd);
            free(data);
            continue;
        }

        SLIST_INSERT_HEAD(&threads, data, entries);

        struct client_thread *current;
        struct client_thread *next;

        current = SLIST_FIRST(&threads);

        while (current != NULL)
        {
            next = SLIST_NEXT(current, entries);

            if (current->complete)
            {
                pthread_join(current->thread, NULL);

                SLIST_REMOVE(&threads,
                             current,
                             client_thread,
                             entries);

                free(current);
            }

            current = next;
        }
    }

    close(sockfd);

    struct client_thread *current;

    while (!SLIST_EMPTY(&threads))
    {
        current = SLIST_FIRST(&threads);

        if (!current->complete)
        {
            shutdown(current->client_fd, SHUT_RDWR);
        }

        pthread_join(current->thread, NULL);

        SLIST_REMOVE_HEAD(&threads, entries);

        free(current);
    }

#if !USE_AESD_CHAR_DEVICE
    if (timestamp_thread_started)
    {
        pthread_join(timestamp_thread, NULL);
    }

    remove(DATA_PATH);

    pthread_mutex_destroy(&file_mutex);
#endif

    return 0;
}
