#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#define RD_BUFF_MAX 128
#define CLIENT_MAX 400

char *
    types[9][2] = {
        {"html", "text/html"},
        {"js", "application/javascript"},
        {"txt", "text/htm"},
        {"css", "text/css"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"swf", "application/x-shockwave-flash"}};

typedef struct myhttp_header
{
    char method[5];
    char filename[200];
    char type[100];
} myhttp_header;

typedef struct Task
{
    int sockfd;
    struct Task *next;
} Task;

typedef struct TaskQueue
{
    Task *head;
    Task *tail;
} TaskQueue;

TaskQueue taskQueue;

int taskCount = 0;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

void error_handle(char *err)
{
    perror(err);
    exit(EXIT_FAILURE);
}

char from_hex(char ch)
{
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

char *url_decode(char *str)
{
    char *pstr = str, *buf = malloc(strlen(str) + 1), *pbuf = buf;
    while (*pstr)
    {
        if (*pstr == '%')
        {
            if (pstr[1] && pstr[2])
            {
                *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
                pstr += 2;
            }
        }
        else if (*pstr == '+')
        {
            *pbuf++ = ' ';
        }
        else
        {
            *pbuf++ = *pstr;
        }
        pstr++;
    }
    *pbuf = '\0';

    return buf;
}

char *get_content_type(char *file_type)
{
    for (size_t i = 0; i < 9; ++i)
    {
        if (strcmp(file_type, types[i][0]) == 0)
        {
            return types[i][1];
        }
    }

    return "*/*";
}

int parse_http_header(char *buff, struct myhttp_header *header)
{
    char *tmp;
    char section[strlen(buff)];
    strcpy(section, buff);

    tmp = strstr(section, " ");
    if (tmp == NULL)
    {
        return -1;
    }
    memcpy(header->method, section, strlen(buff) - strlen(tmp));
    header->method[strlen(buff) - strlen(tmp)] = '\0';

    tmp = strstr(buff, "HTTP/");
    if (tmp == NULL)
    {
        return -1;
    }
    memcpy(header->filename, section + strlen(header->method) + 1, strlen(buff) - strlen(header->method) - strlen(tmp) - 2);
    header->filename[strlen(buff) - strlen(header->method) - strlen(tmp) - 2] = '\0';

    if (strstr(header->filename, "../"))
    {
        return -2;
    }

    if (strchr(header->filename, '%') != NULL)
    {
        char *decoded_url;
        decoded_url = url_decode(header->filename);
        strcpy(header->filename, decoded_url);
        free(decoded_url);
    }

    tmp = strchr(header->filename, '?');
    if (tmp != NULL)
    {
        memcpy(header->filename, header->filename, strlen(header->filename) - strlen(tmp));
        header->filename[strlen(header->filename) - strlen(tmp)] = '\0';
    }

    if (header->filename[strlen(header->filename) - 1] == '/')
    {
        if (strchr(header->filename, '.'))
        {
            return -1;
        }
        strcat(header->filename, "index.html");
    }

    tmp = strrchr(header->filename, '.');
    if (tmp != NULL)
    {
        strcpy(header->type, get_content_type(tmp + 1));
    }

    return 0;
}

void send_header(int sockfd, char *method, char *code, char *type, int length)
{
    char buf[450];

    time_t current_time;
    char *c_time_string;

    current_time = time(NULL);
    c_time_string = ctime(&current_time);
    c_time_string[strlen(c_time_string) - 1] = '\0';

    if (strcmp(code, "200") == 0)
    {
        sprintf(buf,
                "HTTP/1.0 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Server: awesome_http_server\r\n"
                "Content-Length: %d\r\n"
                "Connection: close\r\n"
                "Date: %s\r\n\r\n",
                type, length, c_time_string);
    }
    else if (strcmp(code, "400") == 0)
    {
        strcpy(buf, "HTTP/1.0 404 Bad Request\r\n"
                    "Server: awesome_http_server\r\n\r\n");
    }
    else if (strcmp(code, "403") == 0)
    {
        strcpy(buf, "HTTP/1.0 403 Forbidden\r\n"
                    "Server: awesome_http_server\r\n\r\n");
    }
    else if (strcmp(code, "404") == 0)
    {
        strcpy(buf, "HTTP/1.0 404 Not Found\r\n"
                    "Server: awesome_http_server\r\n\r\n");
    }
    else if (strcmp(code, "405") == 0)
    {
        strcpy(buf, "HTTP/1.0 405 Not Found\r\n"
                    "Server: awesome_http_server\r\n\r\n");
    }

    send(sockfd, buf, strlen(buf), 0);
}

void send_response(int sockfd, struct myhttp_header *header)
{
    FILE *file;
    char code[4];
    char *tmp;

    file = fopen(header->filename + 1, "r");
    if (file == NULL)
    {
        if (errno == ENOENT)
        {
            tmp = strstr(header->filename, "index");
            if (tmp == NULL)
            {
                strcpy(code, "404");
            }
            else
            {
                strcpy(code, "403");
            }
            strcpy(code, "404");
        }
        else if (errno == EACCES)
        {
            strcpy(code, "403");
        }
        else
        {
            strcpy(code, "404");
        }

        send_header(sockfd, header->method, code, header->type, 0);
        printf("%s %s %s\n", header->method, header->filename, code);
    }
    else
    {
        fseek(file, 0L, SEEK_END);
        int size = ftell(file);

        fseek(file, 0L, SEEK_SET);

        if (strcmp(header->method, "HEAD") == 0)
        {
            strcpy(code, "200");
            send_header(sockfd, header->method, code, header->type, size);
            printf("%s %s %s\n", header->method, header->filename, code);

            fclose(file);
            return;
        }

        if (strcmp(header->method, "POST") == 0)
        {
            strcpy(code, "405");
            send_header(sockfd, header->method, code, header->type, size);
            printf("%s %s %s\n", header->method, header->filename, code);

            fclose(file);
            return;
        }

        strcpy(code, "200");
        send_header(sockfd, header->method, code, header->type, size);
        printf("%s %s %s\n", header->method, header->filename, code);

        int nbytes = 0;
        char filedata[size];
        while (size > 0)
        {
            nbytes = fread(filedata, sizeof(char), size - nbytes, file);
            if (nbytes == -1)
            {
                fclose(file);
                return;
            }
            size -= nbytes;

            ssize_t nwrite = 0;
            nwrite = write(sockfd, filedata, nbytes);
            if (nwrite == -1)
            {
                fclose(file);
                return;
            }
            nbytes -= nwrite;
            while (nbytes > 0)
            {
                nwrite = write(sockfd, filedata + nwrite, nbytes);
                if (nwrite == -1)
                {
                    fclose(file);
                    return;
                }
                nbytes -= nwrite;
            }
        }

        fclose(file);
    }
}

int read_from_client(int sockfd)
{
    char *buffer = malloc(sizeof(char) * RD_BUFF_MAX);
    int nbytes = 0;
    int nread;
    struct myhttp_header header;

    nread = read(sockfd, buffer, RD_BUFF_MAX);
    if (nread == 0)
    {
        puts("error reading from socket");
        send_header(sockfd, NULL, "404", NULL, 0);
        printf("Empty request %s\n", "404");
        close(sockfd);
        free(buffer);
        return -1;
    }

    nbytes = nread;
    if (strstr(buffer, "\r\n\r\n") == NULL)
    {
        while (nread > 0)
        {
            buffer = (char *)realloc(buffer, nbytes + RD_BUFF_MAX);
            nread = read(sockfd, buffer + nbytes, RD_BUFF_MAX);
            nbytes += nread;
        }
    }
    if (nread == -1)
    {
        send_header(sockfd, NULL, "404", NULL, 0);
        close(sockfd);
        free(buffer);
        return -1;
    }

    buffer[nbytes + 1] = '\0';
    int err = 0;
    err = parse_http_header(buffer, &header);
    if (err == -1)
    {
        send_header(sockfd, NULL, "404", NULL, 0);
        close(sockfd);
        free(buffer);
        return -1;
    }
    if (err == -2)
    {
        send_header(sockfd, NULL, "403", NULL, 0);
        close(sockfd);
        free(buffer);
        return -1;
    }

    send_response(sockfd, &header);
    close(sockfd);
    free(buffer);
    return 0;
}

void push(Task *task)
{
    pthread_mutex_lock(&mutexQueue);
    if (taskCount == 0)
    {
        taskQueue.head = task;
        taskQueue.tail = task;
    }
    else
    {
        taskQueue.tail->next = task;
        taskQueue.tail = task;
    }
    taskCount++;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueue);
}

Task *pop()
{
    Task *buf = taskQueue.head;
    if (taskCount == 1)
    {
        taskQueue.head = NULL;
        taskQueue.tail = NULL;
    }
    else
    {
        taskQueue.head = taskQueue.head->next;
    }

    taskCount--;
    return buf;
}

void *startThread(void *args)
{
    while (1)
    {
        Task *task;

        pthread_mutex_lock(&mutexQueue);
        while (taskCount == 0)
        {
            pthread_cond_wait(&condQueue, &mutexQueue);
        }

        task = pop();
        pthread_mutex_unlock(&mutexQueue);

        int err = read_from_client(task->sockfd);
        if (err != 0)
        {
            free(task);
            continue;
        }
        free(task);
    }
}

int main(int argc, char *argv[])
{
    int myhttpd_sockfd, client_sockfd;
    int myhttpd_port;
    int threads_amount = 0;

    struct sockaddr_in myhttpd_sockaddr, client_sockaddr;
    socklen_t size;

    switch (argc)
    {
    case 1:
    {
        threads_amount = sysconf(_SC_NPROCESSORS_ONLN);
        myhttpd_port = 8080;
        break;
    }
    case 2:
    {
        threads_amount = sysconf(_SC_NPROCESSORS_ONLN);
        myhttpd_port = atoi(argv[1]);
        break;
    }
    case 3:
    {
        threads_amount = atoi(argv[2]);
        myhttpd_port = atoi(argv[1]);
        break;
    }
    default:
        puts("Wrong number of arguments");
    }

    myhttpd_sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (myhttpd_sockfd < 0)
    {
        error_handle("myhttpd socket failed to create");
    }

    if (setsockopt(myhttpd_sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
    {
        close(myhttpd_sockfd);
        error_handle("bind failed");
    }

    memset(&myhttpd_sockaddr, 0, sizeof(myhttpd_sockaddr));
    myhttpd_sockaddr.sin_family = AF_INET;
    myhttpd_sockaddr.sin_port = htons(myhttpd_port);
    myhttpd_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(myhttpd_sockfd, (struct sockaddr *)&myhttpd_sockaddr,
             sizeof(myhttpd_sockaddr)) == -1)
    {
        close(myhttpd_sockfd);
        error_handle("bind failed");
    }

    printf("Listening port: %d\n", myhttpd_port);
    printf("Threads amount: %d\n", threads_amount);

    if (listen(myhttpd_sockfd, CLIENT_MAX) == -1)
    {
        close(myhttpd_sockfd);
        error_handle("listen failed");
    }

    pthread_t th[threads_amount];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);
    for (size_t i = 0; i < threads_amount; i++)
    {
        if (pthread_create(&th[i], NULL, &startThread, NULL) != 0)
        {
            perror("Failed to create the thread");
        }
    }

    while (1)
    {
        size = sizeof(client_sockaddr);
        client_sockfd = accept(myhttpd_sockfd, (struct sockaddr *)&client_sockaddr, &size);
        if (client_sockfd != -1)
        {
            Task *t = malloc(sizeof(Task));
            t->sockfd = client_sockfd;
            push(t);
        }
    }

    for (size_t i = 0; i < threads_amount; i++)
    {
        if (pthread_join(th[i], NULL) != 0)
        {
            perror("Failed to join the thread");
        }
    }

    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    close(myhttpd_sockfd);

    return 0;
}