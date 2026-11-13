#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "common.h"

#define QUEUE_CAPACITY 4
#define EVENT_COUNT 12

typedef struct {
    int sequence;
    char message[128];
} Event;

typedef struct {
    Event items[QUEUE_CAPACITY];
    int head;
    int tail;
    int count;
    int producer_done;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} EventQueue;

static EventQueue queue_state;

static void enqueue_event(const Event *event)
{
    pthread_mutex_lock(&queue_state.mutex);
    while (queue_state.count == QUEUE_CAPACITY)
        pthread_cond_wait(&queue_state.not_full, &queue_state.mutex);
    queue_state.items[queue_state.tail] = *event;
    queue_state.tail = (queue_state.tail + 1) % QUEUE_CAPACITY;
    ++queue_state.count;
    printf("Producer: queued event %d\n", event->sequence);
    pthread_cond_signal(&queue_state.not_empty);
    pthread_mutex_unlock(&queue_state.mutex);
}

static void *producer_thread(void *unused)
{
    (void)unused;
    for (int i = 1; i <= EVENT_COUNT; ++i) {
        Event event;
        event.sequence = i;
        (void)snprintf(event.message, sizeof(event.message),
                       "event=%02d job=J%d state=%s", i, ((i - 1) % 4) + 1,
                       (i % 3 == 0) ? "COMPLETED" : ((i % 2 == 0) ? "RUNNING" : "READY"));
        enqueue_event(&event);
    }
    pthread_mutex_lock(&queue_state.mutex);
    queue_state.producer_done = 1;
    pthread_cond_broadcast(&queue_state.not_empty);
    pthread_mutex_unlock(&queue_state.mutex);
    return NULL;
}

static void *consumer_thread(void *unused)
{
    (void)unused;
    FILE *log_file = fopen("campuscompute.log", "w");
    if (log_file == NULL) {
        perror("fopen campuscompute.log");
        pthread_mutex_lock(&queue_state.mutex);
        queue_state.producer_done = 1;
        pthread_cond_broadcast(&queue_state.not_full);
        pthread_mutex_unlock(&queue_state.mutex);
        return NULL;
    }

    for (;;) {
        Event event;
        pthread_mutex_lock(&queue_state.mutex);
        while (queue_state.count == 0 && !queue_state.producer_done)
            pthread_cond_wait(&queue_state.not_empty, &queue_state.mutex);
        if (queue_state.count == 0 && queue_state.producer_done) {
            pthread_mutex_unlock(&queue_state.mutex);
            break;
        }
        event = queue_state.items[queue_state.head];
        queue_state.head = (queue_state.head + 1) % QUEUE_CAPACITY;
        --queue_state.count;
        pthread_cond_signal(&queue_state.not_full);
        pthread_mutex_unlock(&queue_state.mutex);

        fprintf(log_file, "[seq=%02d] %s\n", event.sequence, event.message);
        printf("Consumer: wrote event %d to campuscompute.log\n", event.sequence);
    }
    if (fclose(log_file) != 0) perror("fclose campuscompute.log");
    puts("Consumer: queue drained; log file closed.");
    return NULL;
}

void run_logger(void)
{
    pthread_t producer, consumer;
    int producer_created = 0;
    int consumer_created = 0;

    memset(&queue_state, 0, sizeof(queue_state));
    if (pthread_mutex_init(&queue_state.mutex, NULL) != 0 ||
        pthread_cond_init(&queue_state.not_empty, NULL) != 0 ||
        pthread_cond_init(&queue_state.not_full, NULL) != 0) {
        fprintf(stderr, "Logger error: unable to initialize synchronization objects.\n");
        return;
    }

    puts("Starting bounded producer-consumer logger (queue capacity 4, 12 events).");
    if (pthread_create(&consumer, NULL, consumer_thread, NULL) == 0) {
        consumer_created = 1;
    } else {
        fprintf(stderr, "Logger error: could not create consumer thread.\n");
        pthread_mutex_lock(&queue_state.mutex);
        queue_state.producer_done = 1;
        pthread_mutex_unlock(&queue_state.mutex);
    }

    if (consumer_created && pthread_create(&producer, NULL, producer_thread, NULL) == 0) {
        producer_created = 1;
    } else if (consumer_created) {
        fprintf(stderr, "Logger error: could not create producer thread.\n");
        pthread_mutex_lock(&queue_state.mutex);
        queue_state.producer_done = 1;
        pthread_cond_broadcast(&queue_state.not_empty);
        pthread_mutex_unlock(&queue_state.mutex);
    }

    if (producer_created) pthread_join(producer, NULL);
    if (consumer_created) pthread_join(consumer, NULL);

    pthread_cond_destroy(&queue_state.not_full);
    pthread_cond_destroy(&queue_state.not_empty);
    pthread_mutex_destroy(&queue_state.mutex);
    puts("Logger: all created threads joined; synchronization objects destroyed.");
}
