#include "gtest/gtest.h"
#include <memory>

extern "C" {

#include "../scheduler.c"

	struct TestStruct {
		bool is_one_shot = false;
		int n_init = 0;
		int n_run = 0;
		int n_destroy = 0;
		int n_interrupt = 0;
		int n_is_done = 0;
	};

	static void init(void *a) {
		struct TestStruct *s = (struct TestStruct *)a;
		s->n_init++;
	}
	static void run(void *a) {
		struct TestStruct *s = (struct TestStruct *)a;
		s->n_run++;
	}
	static void destroy(void *a) {
		struct TestStruct *s = (struct TestStruct *)a;
		s->n_destroy++;
	}
	static void interrupt(void *a) {
		struct TestStruct *s = (struct TestStruct *)a;
		s->n_interrupt++;
	}
	static bool is_done(void *a) {
		struct TestStruct *s = (struct TestStruct *)a;
		s->n_is_done++;
		return s->n_interrupt > 0 || s->is_one_shot;
	}
}

namespace {
	TEST(SchedulerTest, TaskInit) {
		int data = 0;

		auto t = task_new(init, run, destroy, interrupt, is_done, &data);
		EXPECT_EQ(init, t->init);
		EXPECT_EQ(run, t->run);
		EXPECT_EQ(destroy, t->destroy);
		EXPECT_EQ(interrupt, t->interrupt);
		EXPECT_EQ(is_done, t->is_done);
		EXPECT_EQ(&data, t->data);
		EXPECT_EQ(STARTING, t->state);
		task_free(t);

		t = task_new(NULL, run, NULL, NULL, NULL, NULL);
		EXPECT_EQ(NULL, t->init);
		EXPECT_EQ(run, t->run);
		EXPECT_EQ(NULL, t->destroy);
		EXPECT_EQ(NULL, t->interrupt);
		EXPECT_EQ(NULL, t->is_done);
		EXPECT_EQ(NULL, t->data);
		EXPECT_EQ(STARTING, t->state);
		EXPECT_EQ(STARTING, t->state);
		task_free(t);
}

	TEST(SchedulerTest, SchedulerInit) {
		auto s = scheduler_new();
		EXPECT_NE(nullptr, s);
		EXPECT_EQ(s->tasks.head.next, &s->tasks.tail);
		EXPECT_EQ(s->tasks.tail.prev, &s->tasks.head);
		EXPECT_EQ(NULL, s->tasks.head.prev);
		EXPECT_EQ(NULL, s->tasks.tail.next);
		EXPECT_EQ(0, pthread_mutex_lock(&s->tasks_lock));
		EXPECT_EQ(0, pthread_mutex_unlock(&s->tasks_lock));
		scheduler_free(s);
	}

#define expect_data(DATA, INIT, RUN, DESTROY, INTERRUPT, DONE) {\
		EXPECT_EQ(INIT, DATA.n_init);\
		EXPECT_EQ(RUN, DATA.n_run);\
		EXPECT_EQ(DESTROY, DATA.n_destroy);\
		EXPECT_EQ(INTERRUPT, DATA.n_interrupt);\
		EXPECT_EQ(DONE, DATA.n_is_done);\
}

	TEST(SchedulerTest, StartStop) {
		struct TestStruct data;
		auto t = task_new(init, run, destroy, interrupt, is_done, &data);
		auto s = scheduler_new();

		scheduler_start(s, t);
		expect_data(data, 0, 0, 0, 0, 0);
		EXPECT_EQ(1, list_size(&s->tasks));
		EXPECT_EQ(&t->elem, list_begin(&s->tasks));
		EXPECT_EQ(STARTING, t->state);

		scheduler_run(s);
		expect_data(data, 1, 1, 0, 0, 1);
		EXPECT_EQ(RUNNING, t->state);

		scheduler_stop(s, t);
		expect_data(data, 1, 1, 0, 0, 1);
		EXPECT_FALSE(list_empty(&s->tasks));
		EXPECT_EQ(INTERRUPTED, t->state);

		scheduler_run(s);
		expect_data(data, 1, 1, 0, 1, 1);
		EXPECT_FALSE(list_empty(&s->tasks));
		EXPECT_EQ(STOPPED, t->state);

		scheduler_run(s);
		expect_data(data, 1, 1, 1, 1, 1);
		EXPECT_TRUE(list_empty(&s->tasks));
		EXPECT_EQ(STOPPED, t->state);

		scheduler_free(s);
		expect_data(data, 1, 1, 1, 1, 1);
	}

	TEST(SchedulerTest, RemoveAtFree) {
		struct TestStruct data;
		auto t = task_new(init, run, destroy, interrupt, is_done, &data);
		auto s = scheduler_new();

		scheduler_start(s, t);
		expect_data(data, 0, 0, 0, 0, 0);
		scheduler_run(s);
		expect_data(data, 1, 1, 0, 0, 1);

		scheduler_free(s);
		expect_data(data, 1, 1, 1, 1, 2);

		task_free(t);
	}

	TEST(SchedulerTest, RunNoTasks) {
		// I know, this is redudant and will never fail, But it's only 4 lines
		auto s = scheduler_new();
		scheduler_run(s);
		scheduler_free(s);
		EXPECT_TRUE(true); // ran with no errors, nothing else to test
	}

	TEST(SchedulerTest, RunOneTask) {
		struct TestStruct data;
		auto t = task_new(init, run, destroy, interrupt, is_done, &data);
		auto s = scheduler_new();

		scheduler_start(s, t);
		expect_data(data, 0, 0, 0, 0, 0);

		scheduler_run(s);
		scheduler_run(s);
		expect_data(data, 1, 2, 0, 0, 2);

		scheduler_free(s);
		expect_data(data, 1, 2, 1, 1, 3);

		task_free(t);
	}

	TEST(SchedulerTest, RunToDone) {
		struct TestStruct data = { .is_one_shot = true };
		auto s = scheduler_new();
		auto t = task_new(init, run, destroy, interrupt, is_done, &data);

		scheduler_start(s, t);
		expect_data(data, 0, 0, 0, 0, 0);

		scheduler_run(s);
		expect_data(data, 1, 1, 0, 0, 1);

		scheduler_run(s);
		expect_data(data, 1, 1, 1, 0, 1);

		scheduler_free(s);
		expect_data(data, 1, 1, 1, 0, 1);

		task_free(t);
	}

	TEST(SchedulerTest, RunOneShot) {
		struct TestStruct data = { .is_one_shot = true };
		auto t = task_new(NULL, run, NULL, NULL, NULL, &data);
		auto s = scheduler_new();

		scheduler_start(s, t);
		expect_data(data, 0, 0, 0, 0, 0);

		scheduler_run(s);
		expect_data(data, 0, 1, 0, 0, 0);
		EXPECT_FALSE(list_empty(&s->tasks));

		scheduler_run(s);
		expect_data(data, 0, 1, 0, 0, 0);
		EXPECT_TRUE(list_empty(&s->tasks));

		scheduler_free(s);
		expect_data(data, 0, 1, 0, 0, 0);

		task_free(t);
	}

	TEST(SchedulerTest, RunMultiTasks) {
		int n = 2;
		struct TestStruct data[n];
		struct task *t[n];
		for (int i = 0; i < n; i++)
			t[i] = task_new(init, run, destroy, interrupt, is_done, &data[i]);

		auto s = scheduler_new();

		for (int i = 0; i < n; i++) {
			scheduler_start(s, t[i]);
			expect_data(data[i], 0, 0, 0, 0, 0);
		}
		EXPECT_EQ(n, list_size(&s->tasks));

		scheduler_run(s);
		for (int i = 0; i < n; i++)
			expect_data(data[i], 1, 1, 0, 0, 1);
		EXPECT_EQ(n, list_size(&s->tasks));

		scheduler_run(s);
		for (int i = 0; i < n; i++)
			expect_data(data[i], 1, 2, 0, 0, 2);
		EXPECT_EQ(n, list_size(&s->tasks));

		scheduler_free(s);
		for (int i = 0; i < n; i++)
			expect_data(data[i], 1, 2, 1, 1, 3);

		for (int i = 0; i < n; i++)
			task_free(t[i]);
	}

	TEST(SchedulerTest, RunMixedTasks) {
		struct TestStruct d0, d1, d2;
		d1.is_one_shot = true;
		d2.is_one_shot = true;
		struct task *t[3];
	    t[0] = task_new(init, run, destroy, interrupt, is_done, &d0);
		t[1] = task_new(init, run, destroy, interrupt, is_done, &d1);
		t[2] = task_new(NULL, run, NULL, NULL, NULL, &d2);

		auto s = scheduler_new();

		for (int i = 0; i < 3; i++)
			scheduler_start(s, t[i]);
		expect_data(d0, 0, 0, 0, 0, 0);
		expect_data(d1, 0, 0, 0, 0, 0);
		expect_data(d2, 0, 0, 0, 0, 0); // has no init

		scheduler_run(s);
		expect_data(d0, 1, 1, 0, 0, 1);
		expect_data(d1, 1, 1, 0, 0, 1);
		expect_data(d2, 0, 1, 0, 0, 0); // has no destroy

		scheduler_run(s);
		expect_data(d0, 1, 2, 0, 0, 2);
		expect_data(d1, 1, 1, 1, 0, 1); // should not run
		expect_data(d2, 0, 1, 0, 0, 0); // should not run

		scheduler_free(s);
		expect_data(d0, 1, 2, 1, 1, 3);
		expect_data(d1, 1, 1, 1, 0, 1); // should not destroy
		expect_data(d2, 0, 1, 0, 0, 0); // should not destroy

		for (int i = 0; i < 3; i++)
			task_free(t[i]);
	}

	TEST(SchedulerTest, StopNoRun) {
		struct TestStruct data;
		auto t = task_new(init, run, destroy, interrupt, is_done, &data);
		auto s = scheduler_new();

		scheduler_start(s, t);
		expect_data(data, 0, 0, 0, 0, 0);
		scheduler_stop(s, t);
		expect_data(data, 0, 0, 0, 0, 0);

		scheduler_free(s);
		expect_data(data, 0, 0, 0, 0, 0);

		task_free(t);
	}

	TEST(SchedulerTest, FreeNoRun) {
		struct TestStruct data;
		auto t = task_new(init, run, destroy, interrupt, is_done, &data);
		auto s = scheduler_new();

		scheduler_start(s, t);
		expect_data(data, 0, 0, 0, 0, 0);

		scheduler_free(s);
		expect_data(data, 0, 0, 0, 0, 0);

		task_free(t);
	}

	// new task
	// free task
	// add a task
	// remove task (remove and free)
	// run w/ no task
	// run w/ one task
	// run w/ multi task
	// run w/ oneshot
	// free w/ no tasks
	// free w/ one task
	// free w/ multi tasks
	// free w/ oneshot run
	// free w/ oneshot not run
}
