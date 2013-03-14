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
 * Tests preemptive behavior
 *
 */

#include <Sked.h>
#include "./utest.h"
#include "./util.h"

TestSuite ts;

struct time_array_t times_5ms = {0};
struct time_array_t times_1s = {0};
bool done;

void task_5ms(void) {
    done = markTime(&times_5ms);
}

void task_1s(void) {
    done = markTime(&times_1s);

    uint32_t start = millis();

    /* Wait 100ms */
    while ((millis() - start) < 100) {
    }

    done = true;
}

/**
 * We want to create one long-running task and make sure it's preempted.
 */
Test(test1, ts) {
    uint32_t tstamps_1s[5];
    uint32_t tstamps_5ms[25];

    times_1s.tstamps = tstamps_1s;
    times_1s.size = 5;

    times_5ms.tstamps = tstamps_5ms;
    times_5ms.size = 25;

    /* done will be set true in task_1s */
    done = false;

    /* Init */
    assertEquals(SKED_E_OK, sked.init(SKED_MODE_PREEMPTIVE, SRC_TIMER1));

    /* 1 task @ 1s with a high priority */
    assertEquals(SKED_E_OK, sked.schedule(1000000, 0, 127, task_1s));
    /* 1 task @ 5ms with a lower priority */
    assertEquals(SKED_E_OK, sked.schedule(5000, 0, 0, task_5ms));

    sked.debugPrintState(&Serial);
    sked.start();

    while (!done) {
        if (millis() > 2000) {
            fail("Timeout occurred");
        }
    }

    /* Should have run once */
    assertEquals(1, times_1s.count);
    /* In the 100ms delay, 5ms task should have run 20 times */
    assertEquals(25, times_5ms.count);
    /* We expect that the 5ms task ran after the 1s task */
    assertTrue(times_1s.tstamps[0] < times_5ms.tstamps[0]);

    for (int i = 0; i < times_1s.count; i++) {
        Serial.println(times_1s.tstamps[i]);
    }

    for (int i = 0; i < times_5ms.count; i++) {
        Serial.println(times_5ms.tstamps[i]);
    }

    Serial.println("Deltas: ");
    for (int i = 1; i < times_5ms.count; i++) {
        Serial.println(times_5ms.tstamps[i]-times_5ms.tstamps[i-1]);
        assertEquals(5000, times_5ms.tstamps[i]-times_5ms.tstamps[i-1]);
    }
}

void setup(void) {
    Serial.begin(115200);
    ts.setup();
}

void loop() {
    ts.run();
    finish();
}

