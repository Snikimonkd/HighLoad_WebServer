#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define THREAD_NUM 4

typedef struct Task {
    int a, b;
    struct Task *next;
} Task;

typedef struct TaskQueue {
    Task *head;
    Task *tail;
} TaskQueue;

TaskQueue taskQueue;

int taskCount = 0;

pthread_mutex_t mutexQueue;
pthread_cond_t condQueue;

void executeTask(Task *task) {
    int result = task->a + task->b;
    printf("The sum of %d and %d is %d\n", task->a, task->b, result);
    free(task);
}

void push(Task *task) {
    pthread_mutex_lock(&mutexQueue);
    if (taskCount == 0) {
        taskQueue.head = task;
        taskQueue.tail = task;
    } else {
        taskQueue.tail->next = task;
        taskQueue.tail = task;
    }
    taskCount++;
    pthread_mutex_unlock(&mutexQueue);
    pthread_cond_signal(&condQueue);
}

Task *pop() {
    Task *buf = taskQueue.head;
    if (taskCount == 1) {
        taskQueue.head = NULL;
        taskQueue.tail = NULL;
    } else {
        taskQueue.head = taskQueue.head->next;
    }

    taskCount--;
    return buf;
}

void *startThread(void *args) {
    while (1) {
        Task *task;

        pthread_mutex_lock(&mutexQueue);
        while (taskCount == 0) {
            pthread_cond_wait(&condQueue, &mutexQueue);
        }

        task = pop();

        pthread_mutex_unlock(&mutexQueue);
        executeTask(task);
    }
}

int main(int argc, char *argv[]) {
    pthread_t th[THREAD_NUM];
    pthread_mutex_init(&mutexQueue, NULL);
    pthread_cond_init(&condQueue, NULL);
    for (int i = 0; i < THREAD_NUM; i++) {
        if (pthread_create(&th[i], NULL, &startThread, NULL) != 0) {
            perror("Failed to create the thread");
        }
    }

    srand(time(NULL));
    for (int i = 0; i < 100; i++) {
        Task *t = malloc(sizeof(Task));
        t->a = rand() % 100;
        t->b = rand() % 100;
        push(t);
    }

    for (int i = 0; i < THREAD_NUM; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join the thread");
        }
    }
    pthread_mutex_destroy(&mutexQueue);
    pthread_cond_destroy(&condQueue);
    return 0;
}