#include "list.h"
#include "scheduler.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct scheduler {
    struct list /* <task> */ tasks;
    pthread_mutex_t tasks_lock;
	pthread_mutex_t state_lock;
};

enum task_state {
    STARTING,
    RUNNING,
    INTERRUPTED,
    STOPPED,
};

struct task {
    struct list_elem elem;

    task_fn_t init;
    task_fn_t run;
    task_fn_t destroy;
    task_fn_t interrupt;
    task_cond_t is_done;
    void *data;

    enum task_state state;
};

struct task *task_new(task_fn_t init,
                      task_fn_t run,
                      task_fn_t destroy,
                      task_fn_t interrupt,
                      task_cond_t is_done,
                      void *data) {
    assert(run != NULL);
    if (is_done || interrupt) {
        assert(is_done && interrupt);
    }

    struct task *task = (struct task *)malloc(sizeof(struct task));
    if (!task) {
        perror("malloc(struct task)");
        return NULL;
    }

    task->init = init;
    task->run = run;
    task->destroy = destroy;
    task->interrupt = interrupt;
    task->is_done = is_done;
    task->data = data;
    task->state = STARTING;

    return task;
}

void task_free(struct task *task) {
    free(task);
}

struct scheduler *scheduler_new() {
    struct scheduler *sched =
        (struct scheduler *)malloc(sizeof(struct scheduler));
    if (!sched) {
        perror("malloc(struct scheduler)");
        return NULL;
    }

    list_init(&sched->tasks);
    pthread_mutex_init(&sched->tasks_lock, NULL);
	pthread_mutex_init(&sched->state_lock, NULL);

    return sched;
}

static struct list_elem *scheduler_remove(struct scheduler *sched,
                                          struct task *task) {
    struct list_elem *next = list_remove(&task->elem);
    if (task->state != STARTING && task->destroy)
        task->destroy(task->data);
    return next;
}

void scheduler_free(struct scheduler *sched) {
    pthread_mutex_lock(&sched->tasks_lock);
    pthread_mutex_lock(&sched->state_lock);
    struct list_elem *e;
    for (e = list_begin(&sched->tasks); e != list_end(&sched->tasks);) {
        struct task *task = list_entry(e, struct task, elem);
        switch (task->state) {
        case RUNNING:
            if (!task->is_done || task->is_done(task->data)) {
                task->state = STOPPED;
                break;
            }
        case INTERRUPTED:
            if (task->interrupt)
                task->interrupt(task->data);
            task->state = STOPPED;
            break;
        }
        e = scheduler_remove(sched, task);
    }

    pthread_mutex_destroy(&sched->tasks_lock);
    pthread_mutex_destroy(&sched->state_lock);
    free(sched);
}

void scheduler_run(struct scheduler *sched) {
    pthread_mutex_lock(&sched->state_lock);

    pthread_mutex_lock(&sched->tasks_lock);
    struct list_elem *e;
    for (e = list_begin(&sched->tasks); e != list_end(&sched->tasks);) {
        struct task *task = list_entry(e, struct task, elem);
        e = list_next(e);
    	pthread_mutex_unlock(&sched->tasks_lock);

        switch (task->state) {
        case STARTING:
            if (task->init)
                task->init(task->data);
            task->state = RUNNING;
        case RUNNING:
            task->run(task->data);
            if (!task->is_done || task->is_done(task->data))
                task->state = STOPPED;
            break;
        case INTERRUPTED:
            if (task->interrupt)
                task->interrupt(task->data);
            task->state = STOPPED;
            break;
        case STOPPED:
            e = scheduler_remove(sched, task);
            break;
        }

        pthread_mutex_lock(&sched->tasks_lock);
    }

    pthread_mutex_unlock(&sched->tasks_lock);
    pthread_mutex_unlock(&sched->state_lock);
}

void scheduler_start(struct scheduler *sched, struct task *task) {
    pthread_mutex_lock(&sched->tasks_lock);
    task->state = STARTING;
    list_push_back(&sched->tasks, &task->elem);
    pthread_mutex_unlock(&sched->tasks_lock);
}

void scheduler_stop(struct scheduler *sched, struct task *task) {
    pthread_mutex_lock(&sched->state_lock);
    if (task->state != STARTING)
		task->state = INTERRUPTED;
    pthread_mutex_unlock(&sched->state_lock);
}
