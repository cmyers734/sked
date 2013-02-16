/*--------------------------------------------------------------------------------
 */
#include <Tasker.h>

#define LED 13

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Tasking Example ---\n");
 
  pinMode(LED, OUTPUT);
  
  tasker.init(SRC_TIMER1);
  tasker.schedule(1000000, 0, 0, task_1s);
  tasker.schedule(500000, 200000, 0, task_500ms_phased_200ms);

  tasker.start();
}

void loop() {
}

void task_1s(void) {
  Serial.println("1s Task Executed");
}

void task_500ms_phased_200ms(void) {
  static uint8_t level = 0;
  digitalWrite(LED, level);
  level ^= 0x01;
}

