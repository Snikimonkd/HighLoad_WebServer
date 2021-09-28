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

#define THREAD_NUM 1

#define RD_BUFF_MAX 1024
#define CLIENT_MAX 10

char *types[9][2] = {
    {"js", "application/javascript"},
    {"html", "text/html"},
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
    char *section, *tmp;
    int i;

    section = strtok(buff, " ");
    if (section == NULL)
    {
        return -1;
    }
    strcpy(header->method, section);
    section = strtok(NULL, " ");
    if (section == NULL)
    {
        return -1;
    }

    if (strchr(section, '%') != NULL)
    {
        char *decoded_url;
        decoded_url = url_decode(section);
        puts(decoded_url);
        strcpy(header->filename, decoded_url);
        free(decoded_url);
    }
    else
    {
        strcpy(header->filename, section);
    }

    if (header->filename[strlen(header->filename) - 1] == '/')
    {
        if (strchr(header->filename, '.'))
        {
            return -1;
        }
        strcat(header->filename, "index.html");
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
        send(sockfd, buf, strlen(buf), 0);
        printf("[%s]", buf);
    }
    else if (strcmp(code, "400") == 0)
    {
        strcpy(buf, "HTTP/1.0 404 Bad Request\r\n"
                    "Server: awesome_http_server\r\n\r\n");
        send(sockfd, buf, strlen(buf), 0);
        printf("[%s]", buf);
    }
    else if (strcmp(code, "403") == 0)
    {
        strcpy(buf, "HTTP/1.0 403 Forbidden\r\n"
                    "Server: awesome_http_server\r\n\r\n");
        send(sockfd, buf, strlen(buf), 0);
        printf("[%s]", buf);
    }
    else if (strcmp(code, "404") == 0)
    {
        strcpy(buf, "HTTP/1.0 404 Not Found\r\n"
                    "Server: awesome_http_server\r\n\r\n");
        send(sockfd, buf, strlen(buf), 0);
        printf("[%s]", buf);
    }
}

void send_response(int sockfd, struct myhttp_header *header)
{
    puts("response");
    FILE *file;
    char filename[200];
    char filedata[8092];
    char code[4];
    int len;
    int nbytes = 0;
    char *tmp;
    char new[200];

    /*if (strstr(filename, "%20") != NULL)
    {
        tmp = strtok(filename, "%20");
        strcpy(new, tmp);
        strcat(new, " ");

        while ((tmp = strtok(NULL, "%20")) != NULL)
        {
            strcat(new, tmp);
            strcat(new, " ");
        }

        new[strlen(new) - 1] = 0;
        strcpy(filename, new);
    }*/

    puts("response 2");
    puts(header->filename);
    if (strcmp(header->filename, "."))
    {
        char type[400];
        strcpy(type, header->filename);
        tmp = strtok(type, ".");
        tmp = strtok(NULL, "");
        strcpy(header->type, get_content_type(tmp));
    }

    puts("response 3");
    strcpy(header->filename, header->filename + 1);
    file = fopen(header->filename, "r");
    if (file == NULL)
    {
        if (errno == ENOENT)
        {
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
    }
    else
    {
        fseek(file, 0L, SEEK_END);
        int size = ftell(file);

        fseek(file, 0L, SEEK_SET);

        if (strcmp(header->method, "HEAD") == 0)
        {
            send_header(sockfd, header->method, code, header->type, size);
            fclose(file);
            return;
        }

        strcpy(code, "200");
        send_header(sockfd, header->method, code, header->type, size);
        while ((nbytes = fread(filedata, sizeof(char), 8092, file)) > 0)
        {
            write(sockfd, filedata, nbytes);
        }

        fclose(file);
    }
}

int read_from_client(int sockfd)
{
    char buffer[RD_BUFF_MAX];
    int nbytes;
    struct myhttp_header header;

    nbytes = read(sockfd, buffer, RD_BUFF_MAX);
    if (nbytes < 0)
    {
        error_handle("socket read failed");
        return -1;
    }
    else if (nbytes == 0)
    {
        return -1;
    }
    else
    {
        puts("____________________________________________________");
        buffer[nbytes] = '\0';
        printf("[%s]", buffer);
        int err = 0;
        err = parse_http_header(buffer, &header);
        if (err != 0)
        {
            send_header(sockfd, NULL, "404", NULL, 0);
        }
        else
        {
            send_response(sockfd, &header);
        }
        close(sockfd);
        return 0;
    }
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
        read_from_client(task->sockfd);
        free(task);
    }
}

int main(int argc, char *argv[])
{
    int myhttpd_sockfd, client_sockfd;
    int myhttpd_port;

    struct sockaddr_in myhttpd_sockaddr, client_sockaddr;
    socklen_t size;
    pthread_t newthread;

    if (argc != 2)
    {
        error_handle("incorrect number of args");
    }

    myhttpd_port = atoi(argv[1]);

    myhttpd_sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (myhttpd_sockfd < 0)
    {
        error_handle("myhttpd socket failed to create");
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

    if (listen(myhttpd_sockfd, CLIENT_MAX) == -1)
    {
        close(myhttpd_sockfd);
        error_handle("listen failed");
    }

    pthread_t th[THREAD_NUM];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);
    for (size_t i = 0; i < THREAD_NUM; i++)
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
        if (client_sockfd == -1)
        {
            error_handle("accept error");
        }

        Task *t = malloc(sizeof(Task));
        t->sockfd = client_sockfd;
        push(t);
    }

    for (size_t i = 0; i < THREAD_NUM; i++)
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