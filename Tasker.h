#ifndef TASKER_H
#define TASKER_H

#include <Platform.h>

#define TASKER_MAX_TASKS	16

#define TASKER_E_OK	0
#define TASKER_E_NOT_INITIALIZED -1
#define TASKER_E_TOO_MANY_TASKS	-2
#define TASKER_E_INVALID_PERIOD -3
#define TASKER_E_INVALID_FUNCTION -4
#define TASKER_E_INVALID_PHASE -5
#define TASKER_E_INVALID_PRIORITY -6
#define TASKER_E_INVALID_OPERATION -7
#define TASKER_E_NOT_IMPLEMENTED -99

#define TASKER_OVERRUNS_MAX 255U
#define TASKER_MISSES_MAX   255U

#define TASKER_MIN_PRIORITY -127

typedef enum {
	IDLE = 0,
	READY,
	RUNNING
} tasker_task_state_e;

typedef enum {
	SRC_TIMER1 = 0,
} tasker_clk_src_e;

typedef enum {
	TASKER_MODE_PREEMPTIVE = 0,
	TASKER_MODE_NON_PREEMPTIVE,
} tasker_mode_e;

typedef void (*tasker_task_fcn_t)(void);

typedef struct {
	tasker_task_fcn_t fcn;
	uint16_t count;
	uint16_t period;
	uint16_t phase;
	uint8_t misses;
	uint8_t overruns;
	int8_t priority;
	tasker_task_state_e state;
} tasker_task_t;

class Tasker {
private:
	uint8_t _state;
	uint32_t _max_period_us;
	tasker_clk_src_e _clk_src;
	tasker_task_t _tasks[TASKER_MAX_TASKS];
	uint8_t _task_count;
	int8_t _current_task_priority;
	tasker_mode_e _mode;

public:
	Tasker();
	void debugPrintState(Stream *stream);
	int8_t init(tasker_mode_e mode, tasker_clk_src_e clk_src);
	int8_t schedule(uint32_t period_us, uint32_t phase, int8_t priority, 
        tasker_task_fcn_t fcn);
	void timerISR(void);
	int8_t start(void);
	void loop(void);
};

extern Tasker tasker;

#endif /* TASKER_H */
