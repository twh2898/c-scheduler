#include "list.h"
#include "scheduler.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct scheduler {
    struct list /* <task> */ tasks;
    pthread_mutex_t tasks_lock;
};

struct task {
    struct list_elem elem;

    task_fn_t init;
    task_fn_t run;
    task_fn_t destroy;
    task_fn_t interrupt;
    task_cond_t is_done;
    void *data;
};

struct task *task_new(task_fn_t init,
                      task_fn_t run,
                      task_fn_t destroy,
                      task_fn_t interrupt,
                      task_cond_t is_done,
                      void *data) {
    assert(run != NULL);
    if (is_done) {
        assert(interrupt != NULL);
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

    return sched;
}

static struct list_elem *scheduler_remove(struct scheduler *sched,
                                          struct task *task) {
    struct list_elem *next = list_remove(&task->elem);

    if (task->destroy) {
        task->destroy(task->data);
    }

    return next;
}

void scheduler_free(struct scheduler *sched) {
	pthread_mutex_lock(&sched->tasks_lock);
    struct list_elem *e;
    for (e = list_begin(&sched->tasks); e != list_end(&sched->tasks);) {
        struct task *task = list_entry(e, struct task, elem);
        e = scheduler_remove(sched, task);
    }

    pthread_mutex_destroy(&sched->tasks_lock);
    free(sched);
}

void scheduler_run(struct scheduler *sched) {
    pthread_mutex_lock(&sched->tasks_lock);

    struct list_elem *e;
    for (e = list_begin(&sched->tasks); e != list_end(&sched->tasks);) {
        struct task *task = list_entry(e, struct task, elem);
        e = list_next(e);

        pthread_mutex_unlock(&sched->tasks_lock);
        task->run(task->data);
        pthread_mutex_lock(&sched->tasks_lock);

        if (task->is_done == NULL || task->is_done(task->data)) {
            e = scheduler_remove(sched, task);
        }
    }

    pthread_mutex_unlock(&sched->tasks_lock);
}

void scheduler_add(struct scheduler *sched, struct task *task) {
    pthread_mutex_lock(&sched->tasks_lock);
    list_push_back(&sched->tasks, &task->elem);
    pthread_mutex_unlock(&sched->tasks_lock);

    if (task->init) {
        task->init(task->data);
    }
}
