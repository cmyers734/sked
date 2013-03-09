#ifndef SKED_H
#define SKED_H

#include <Platform.h>

#define SKED_MAX_TASKS	16

#define SKED_E_OK	0
#define SKED_E_NOT_INITIALIZED -1
#define SKED_E_TOO_MANY_TASKS	-2
#define SKED_E_INVALID_PERIOD -3
#define SKED_E_INVALID_FUNCTION -4
#define SKED_E_INVALID_OFFSET -5
#define SKED_E_INVALID_PRIORITY -6
#define SKED_E_INVALID_OPERATION -7
#define SKED_E_NOT_IMPLEMENTED -99

#define SKED_OVERRUNS_MAX 255U
#define SKED_MISSES_MAX   255U

#define SKED_MIN_PRIORITY -127

typedef enum {
	IDLE = 0,
	READY,
	RUNNING
} sked_task_state_e;

typedef enum {
	SRC_TIMER1 = 1,
} sked_clk_src_e;

typedef enum {
	SKED_MODE_PREEMPTIVE = 0,
	SKED_MODE_NON_PREEMPTIVE,
} sked_mode_e;

typedef void (*sked_task_fcn_t)(void);

typedef struct {
	sked_task_fcn_t fcn;
	uint16_t count;
	uint16_t period;
	uint16_t offset;
	uint8_t misses;
	uint8_t overruns;
	int8_t priority;
	sked_task_state_e state;
} sked_task_t;

class Sked {
private:
	uint8_t _state;
	uint32_t _min_period_us;
	uint32_t _max_period_us;
	sked_clk_src_e _clk_src;
	sked_task_t _tasks[SKED_MAX_TASKS];
	uint8_t _task_count;
	int8_t _current_task_priority;
	sked_mode_e _mode;

public:
	Sked();
	void debugPrintState(Stream *stream);
	int8_t init(sked_mode_e mode, sked_clk_src_e clk_src);
	int8_t schedule(uint32_t period_us, uint32_t offset_us, int8_t priority, 
        sked_task_fcn_t fcn);
	void timerISR(void);
	void reset(void);
	int8_t start(void);
	int8_t loop(void);
	sked_task_t *getTaskInfo(uint8_t i);
	uint8_t getTaskCount(void);
};

extern Sked sked;

#endif /* SKED_H */
