/**
 * Test 1 - Just the basics. Schedule a 1 second task and make sure it
 * completes on-time
 */

#include <Sked.h>
#include "utest.h"
#include "util.h"

TestSuite ts;

struct time_array_t times_1s = {0};
bool done;

void task_1s(void) {
    static uint8_t val = 0;
    digitalWrite(12, val);
    done = markTime(&times_1s);
    val ^= 1;
}

Test(test1, ts) {
    uint32_t tstamps[5];
    times_1s.tstamps = tstamps;
    times_1s.size = 5;
    
    /* For debugging, look at pin 12 */
    pinMode(12, OUTPUT);
    
    /* done will be set true in task_1s */
    done = false;

    /* 1 task @ 1s */
    assertEquals(SKED_E_OK, sked.init(SKED_MODE_PREEMPTIVE, SRC_TIMER1));
    assertEquals(SKED_E_OK, sked.schedule(1000000, 0, 0, task_1s));

    sked.debugPrintState(&Serial);
    sked.start();

    while (!done) {
        if (millis() > 7000) {
            fail("Timeout occurred");
        }
    }

    for (int i = 0; i < times_1s.count; i++) {
        Serial.println(times_1s.tstamps[i]);
    }

    Serial.println("Deltas: ");
    for (int i = 1; i < times_1s.count; i++) {
        Serial.println(times_1s.tstamps[i]-times_1s.tstamps[i-1]);
        assertEquals(1000000, times_1s.tstamps[i]-times_1s.tstamps[i-1]);
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

