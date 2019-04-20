#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * The scheduler system.
 */
struct scheduler;

/**
 * A task in the scheduler system
 */
struct task;

typedef void (*task_fn_t)(void *);
typedef bool (*task_cond_t)(void *);

/**
 * Generate a new task. The run fn will be called periodically while the task
 * is active. The init and destroy fn's are optional, pass NULL to disable. If
 * is_done is NULL, this task will be a one-shot (ie. run will be called once).
 * If is_done is present, then interrupt must also be present. interrupt must
 * stop the task or put the task into some state where is_done returns true.
 */
struct task *task_new(task_fn_t init,
                      task_fn_t run,
                      task_fn_t destroy,
                      task_fn_t interrupt,
                      task_cond_t is_done,
                      void *data);

void task_free(struct task *task);

// scheduler_new and scheduler_run and scheduler_free should be called from the
// same thread.

struct scheduler *scheduler_new(void);

void scheduler_free(struct scheduler *);

void scheduler_run(struct scheduler *);

void scheduler_add(struct scheduler *, struct task *);

#ifdef __cplusplus
}
#endif
