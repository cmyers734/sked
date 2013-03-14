/**
 * Sked: Task scheduling library for Arduino.
 *
 * Copyright (c) 2013, Christopher Myers.  All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * --------------------------------------------------------------------------
 *
 * Test the basics of task insertion and sorting along with testing return
 * values and run-time checking.
 */

#include <Sked.h>
#include "./utest.h"
#include "./util.h"

TestSuite ts;

void task_stub(void) {
}

/**
 * Test init() function
 */
Test(test_init, ts) {
    sked.reset();

    /* Should not be able to start() until init() */
    assertEquals(SKED_E_NOT_INITIALIZED, sked.start());

    /* Test return codes */
    assertEquals(SKED_E_OK, sked.init(SKED_MODE_PREEMPTIVE, SKED_SRC_TIMER1));
    assertEquals(SKED_E_NOT_IMPLEMENTED,
            sked.init(SKED_MODE_PREEMPTIVE, (sked_clk_src_e)0));
    assertEquals(SKED_E_NOT_IMPLEMENTED,
            sked.init(SKED_MODE_PREEMPTIVE, (sked_clk_src_e)2));
    assertEquals(SKED_E_OK, sked.init(SKED_MODE_NON_PREEMPTIVE,
            SKED_SRC_TIMER1));

    /* Test that we have 0 tasks */
    assertEquals(0, sked.getTaskCount());

    /* Should be able to start() now */
    assertEquals(SKED_E_OK, sked.start());
}

/**
 * Test task scheduling rules
 */
Test(test_schedule_rules, ts) {
    sked.reset();

    /* Initialize first to establish max and min periods */
    assertEquals(SKED_E_OK, sked.init(SKED_MODE_PREEMPTIVE, SKED_SRC_TIMER1));

    /* Can't have 0 period */
    assertEquals(SKED_E_INVALID_PERIOD, sked.schedule(0, 0, 0, task_stub));
    assertEquals(0, sked.getTaskCount());

    /* Can't have period less than the min (100us) */
    assertEquals(SKED_E_INVALID_PERIOD, sked.schedule(99, 0, 0, task_stub));
    assertEquals(0, sked.getTaskCount());

    /* Can't have period greater than the max */
    assertEquals(SKED_E_INVALID_PERIOD, sked.schedule(6553500 + 1, 0, 0,
            task_stub));
    assertEquals(0, sked.getTaskCount());

    /* Can't have offset less than the min (100us) unless it's 0 */
    assertEquals(SKED_E_INVALID_OFFSET, sked.schedule(100, 99, 0, task_stub));
    assertEquals(0, sked.getTaskCount());
    assertEquals(SKED_E_OK, sked.schedule(100, 100, 0, task_stub));
    assertEquals(1, sked.getTaskCount());

    /* Undo task add */
    sked.reset();
    assertEquals(SKED_E_OK, sked.init(SKED_MODE_PREEMPTIVE, SKED_SRC_TIMER1));

    /* Can't have offset greater than the max */
    assertEquals(SKED_E_INVALID_OFFSET, sked.schedule(100, 6553500+1, 0,
        task_stub));
    assertEquals(0, sked.getTaskCount());

    /* Priority has to be greater than the min */
    assertEquals(SKED_E_INVALID_PRIORITY,
            sked.schedule(100, 0, SKED_MIN_PRIORITY, task_stub));
    assertEquals(0, sked.getTaskCount());

    /* Can't have null function */
    assertEquals(SKED_E_INVALID_FUNCTION, sked.schedule(100, 0, 0, NULL));
    assertEquals(0, sked.getTaskCount());

    /* Normal schedule OK */
    assertEquals(SKED_E_OK, sked.schedule(100, 0, 0, task_stub));
    assertEquals(1, sked.getTaskCount());

    /* Undo task add */
    sked.reset();
    assertEquals(SKED_E_OK, sked.init(SKED_MODE_PREEMPTIVE, SKED_SRC_TIMER1));

    /* Too many tasks */
    for (int i = 0; i < SKED_MAX_TASKS; i++) {
        assertEquals(SKED_E_OK, sked.schedule(100, 0, 0, task_stub));
        assertEquals(i+1, sked.getTaskCount());
    }
    assertEquals(SKED_E_TOO_MANY_TASKS, sked.schedule(100, 0, 0, task_stub));
}

/**
 * Test that tasks are properly inserted by priority rules
 */
Test(test_prio, ts) {
    sked.reset();

    assertEquals(SKED_E_OK, sked.init(SKED_MODE_PREEMPTIVE, SKED_SRC_TIMER1));
    /* Add task @ 1s with prio 0 */
    assertEquals(SKED_E_OK, sked.schedule(1000000, 0, 0, task_stub));
    /* Should be index 0... no brainer */
    assertEquals(1, sked.getTaskCount());
    assertEquals(10000U, sked.getTaskInfo(0)->period);

    /* Add task @ 1ms with prio 0 */
    assertEquals(SKED_E_OK, sked.schedule(1000, 0, 0, task_stub));
    /* Should be index 0 and 1s task should move to index 1 because
     * for tasks with the same priority, they should be sorted by
     * fastest period first. */
    assertEquals(2, sked.getTaskCount());
    assertEquals(10U, sked.getTaskInfo(0)->period);
    assertEquals(10000U, sked.getTaskInfo(1)->period);

    /* Add a task with a lower priority which should end up at the end
     * of the task list. */
    assertEquals(SKED_E_OK, sked.schedule(100, 0, -1, task_stub));
    assertEquals(3, sked.getTaskCount());
    assertEquals(10U, sked.getTaskInfo(0)->period);
    assertEquals(10000U, sked.getTaskInfo(1)->period);
    assertEquals(1U, sked.getTaskInfo(2)->period);

    /* Add a task with a higher priority which should end up at the head
     * of the task list. */
    assertEquals(SKED_E_OK, sked.schedule(200, 0, 127, task_stub));
    assertEquals(4, sked.getTaskCount());
    assertEquals(2U, sked.getTaskInfo(0)->period);
    assertEquals(10U, sked.getTaskInfo(1)->period);
    assertEquals(10000U, sked.getTaskInfo(2)->period);
    assertEquals(1U, sked.getTaskInfo(3)->period);

    /* Add a task with a priority between 0 and 127 so it gets inserted in the
     * middle. */
    assertEquals(SKED_E_OK, sked.schedule(400, 0, 63, task_stub));
    assertEquals(5, sked.getTaskCount());
    assertEquals(2U, sked.getTaskInfo(0)->period);
    assertEquals(4U, sked.getTaskInfo(1)->period);
    assertEquals(10U, sked.getTaskInfo(2)->period);
    assertEquals(10000U, sked.getTaskInfo(3)->period);
    assertEquals(1U, sked.getTaskInfo(4)->period);

    sked.debugPrintState(&Serial);
    sked.start();
}

void setup(void) {
    Serial.begin(115200);
    ts.setup();
}

void loop() {
    ts.run();
    finish();
}

