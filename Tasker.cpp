/**
 * Tasking library for Arduino.
 *
 * This library provides asynchronous task execution for the Uno Arduino 
 * platform, although it's possible that this works just fine on other
 * boards.
 * 
 * This scheduler is:
 *   - Single stack: that means that as tasks preempt one another, the stack just 
 *     keeps getting deeper. This isn't usually a problem with simpler systems 
 *     like Arduino... it's problematic only in degenerate cases.
 *     Contrast with schedulers/OSes that have a stack allocated per task.
 *     Before changing task, the stack pointer is moved to that task's private
 *     stack. It's just more to worry about but has its advantages.
 *   - Preemptive: that means one task can interrupt another and start
 *     executing. This can be really useful! Imagine you have a task that you
 *     need to execute every 1s, but have to sample an analog pin every 10ms.
 *     You want the 10ms task to preempt the 1s task and return when it's
 *     done.
 *     Contrast with non-preemptive. Tasks wait for one another to finish.
 */

#include <util/atomic.h>
#include "Tasker.h"

/* Fixed 100us tick resolution */
#define TASKER_TIMER1_CLK_HZ (8.0/(float)F_CPU)
#define TASKER_TIMER1_TICK_PERIOD_S 0.000100
#define TASKER_TIMER1_TICK_PERIOD_US (TASKER_TIMER1_TICK_PERIOD_S * 1000000.0)
/* The number of ticks needed for TIMER1 to expire at the desired period */
#define TASKER_TIMER1_TICKS_PER_PERIOD (TASKER_TIMER1_TICK_PERIOD_S / TASKER_TIMER1_CLK_HZ)

/* Different states the tasker can be in */
#define TASKER_STATE_UNINIT 0
#define TASKER_STATE_INIT 1

Tasker::Tasker(void) {
    /* Don't bother to initialize any of the task slots because _task_count
     * determines which are valid. */
    _task_count = 0U;
    _max_period_us = 0U;
    _state = TASKER_STATE_UNINIT;
    _current_task_priority = TASKER_MIN_PRIORITY;
    _mode = TASKER_MODE_PREEMPTIVE;
}

int8_t Tasker::init(tasker_mode_e mode, tasker_clk_src_e clk_src) {
    int8_t ret = TASKER_E_OK;

    _mode = mode;
    _clk_src = clk_src;
    
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (_clk_src == SRC_TIMER1) {
            /* Maximum number of us that can fit in 16-bit counter */
            _max_period_us = (uint32_t)((float)0xFFFF 
                    * TASKER_TIMER1_TICK_PERIOD_US);

            /* Set Initial Timer value */
            TCNT1 = 0x0000U;
            
            /* Put timer 1 into "CTC" mode to disconnect from outputs and get a
             * periodic interrupt that we can control with the contents of the
             * ICR1 register. 
             *
             * Set the prescaler to be /8 by only setting CS11
             */
            TCCR1A = 0x00U;
            TCCR1B = (_BV(WGM12) | _BV(WGM13)) | (_BV(CS11));

            /* Target a specific minimum resolution for tasking ticks. Example: at
             * 16MHz and a prescaler of /8, each tick is 500ns.  Since we don't
             * want an interrupt every 500ns, targeting every 100us would mean
             * an interrupt every 200 counts instead. This also means that all
             * periods and phases need to be even multiples of 100us. */
            ICR1 = (uint16_t)TASKER_TIMER1_TICKS_PER_PERIOD;
            
            /* Interrupt in mode 12 will set the TIRF1 flag, leave disabled
             * until the call to start() */
            TIMSK1 = 0x00;
        
            _state = TASKER_STATE_INIT;
        } else {
            ret = TASKER_E_NOT_IMPLEMENTED;
        }
    }

    return ret;
}

void Tasker::loop(void) {
    if (_mode == TASKER_MODE_NON_PREEMPTIVE) {
        /* Search for a task that is ready to run and execute it */
        for (uint8_t i = 0; i < _task_count; i++) {
            tasker_task_t *task = &_tasks[i];

            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                /* We can run a task when it's READY. We can neglect priority
                 * because we sorted tasks by priority upon their insertion into
                 * the task list. By iterating through the tasks from
                 * 0->_task_count, we'll be honoring the priority by running the
                 * READY tasks according to priority. */
                if (task->state == READY) {
                    task->state = RUNNING;

                    /* Enable interrupts to allow for the tick interrupt to occur
                     * again (as well as other interrupts) during the task function's
                     * execution. */
                    NONATOMIC_BLOCK(NONATOMIC_RESTORESTATE) {
                        task->fcn();
                    }

                    task->state = IDLE;
                }
            } /* End of atomic block */
        }
    }
}

void Tasker::timerISR(void) {
    /* This occurs periodically. Walk through each task and update its state. */
    for (uint8_t i = 0; i < _task_count; i++) {
        tasker_task_t *task = &_tasks[i];

        /* Each task has a count which we decrement if we can */
        if (task->count != 0) {
            task->count--;
        }

        /* When it hits zero, that task is ready to run. It can be in any
         * state: 
         *
         * If it's already RUNNING, it means a whole task period went by 
         * and it didn't get a chance to finish in the previous period.
         *
         * If it's IDLE, then great, it's ready to run now
         *
         * If it's READY, it means that an entire period has gone by 
         * without the task being able to execute. */
        if (task->count == 0) {
            if (task->state == IDLE) {
                /* Move it to the "ready to run" state */
                task->state = READY;
            } else if (task->state == RUNNING) {
                /* Overrun */
                task->overruns = constrain(TASKER_OVERRUNS_MAX, 
                        0, task->overruns+1);
            } else {
                /* Miss! */
                task->misses = constrain(TASKER_MISSES_MAX, 
                        0, task->misses+1);
            }
            
            /* Reset the count back to the period. You'll note that we don't
             * care about the phase. Since that was baked in when the task was
             * scheduled, it meant that its first period was phased
             * differently and thus this task will continue to have the 
             * phase baked into each subsequent period without worrying about
             * it. */
            task->count = task->period;
        }
    }

    if (_mode == TASKER_MODE_PREEMPTIVE) {
        /* After we update our state, it's time to execute an available task */
        for (uint8_t i = 0; i < _task_count; i++) {
            tasker_task_t *task = &_tasks[i];

            /* We can run a task when it's ready and is of higher priority than
             * the task we're running already. */
            if ((task->state == READY) && (task->priority > _current_task_priority)) {
                task->state = RUNNING;

                /* Record the priority so we don't let other lower-prio tasks
                 * interrupt us */
                _current_task_priority = task->priority;

                /* Enable interrupts to allow for the tick interrupt to occur
                 * again (as well as other interrupts) during the task function's
                 * execution. */
                NONATOMIC_BLOCK(NONATOMIC_RESTORESTATE) {
                    task->fcn();
                }

                /* Important! Reset the priority back to the lowest possible or
                 * else you'll never execute any tasks with lower priority than
                 * the one we just ran. */
                _current_task_priority = TASKER_MIN_PRIORITY;
                
                task->state = IDLE;
            }
        }
    }
}

void Tasker::debugPrintState(Stream *stream) {
    if (_state == TASKER_STATE_UNINIT) {
        stream->println("### Tasker is UNINITIALIZED.");
    } else {
        stream->println("### Tasker is INITIALIZED.");
        stream->print("### Max Period (us): ");
        stream->println(_max_period_us);
        stream->print("### Src Timer:       ");
        switch (_clk_src) {
            case SRC_TIMER1:
                stream->println("TIMER1");
                stream->print("###    Count:     ");
                stream->println(TCNT1);
                stream->print("###    Max Count: ");
                stream->println(ICR1);
                stream->print("###    TCCR1A:    ");
                stream->println(TCCR1A, HEX);
                stream->print("###    TCCR1B:    ");
                stream->println(TCCR1B, HEX);
                stream->print("###    TIMSK1:    ");
                stream->println(TIMSK1, HEX);
                stream->print("###    Ticks Per Period: ");
                stream->println(TASKER_TIMER1_TICKS_PER_PERIOD);
                break;
            
            default:
                stream->println("INVALID");
                break;
        }

        stream->print("### Tasks: "); stream->println(_task_count);
        for (uint8_t i = 0; i < _task_count; i++) {
            tasker_task_t *task = &_tasks[i];
        
            stream->print("###   Task["); stream->print(i); stream->print("]: ");
            stream->print("(");
            stream->print(task->priority);
            stream->print(", ");
            stream->print(task->period);
            stream->print(", ");
            stream->print(task->phase);
            stream->print(", ");
            stream->print(task->count);
            stream->print(", ");
            stream->print((uint32_t)task->fcn, HEX);
            stream->println(")");
            stream->print("###     State: ");
            stream->println(task->state);
            stream->print("###     Misses: ");
            stream->println(task->misses);
            stream->print("###     Overruns: ");
            stream->println(task->overruns);
        }
    }
}

int8_t Tasker::schedule(uint32_t period_us, uint32_t phase_us, int8_t priority, 
        tasker_task_fcn_t fcn) {
    uint16_t period;
    uint16_t phase;

    /* We may be full */
    if (_task_count >= TASKER_MAX_TASKS) {
        return TASKER_E_TOO_MANY_TASKS;
    }

    /* Check period and phase:
     *  Both must be less than the max.
     *  The period has to be greater than 0 
     *  The phase must be less than the period. */
    if (period_us == 0 || period_us > _max_period_us) {
        return TASKER_E_INVALID_PERIOD;
    }

    /* Don't need to check that the phase exceeds the max because if it did,
     * then the period would have also exceeded them max and we would have
     * caught that in the above check. */
    if (phase_us > period_us) {
        return TASKER_E_INVALID_PHASE;
    }

    /* Priority needs to be > -127 (which we use to designate the lowest
     * possible priority)
     */
    if (priority <= (int8_t)TASKER_MIN_PRIORITY) {
        return TASKER_E_INVALID_PRIORITY;
    }

    /* Task function cannot be NULL */
    if (fcn == (tasker_task_fcn_t)NULL) {
        return TASKER_E_INVALID_FUNCTION;
    }
    
    /* Convert period and phase to ticks of the timer ISR */
    period = period_us / (uint32_t)(TASKER_TIMER1_TICK_PERIOD_US);
    phase = phase_us / (uint32_t)(TASKER_TIMER1_TICK_PERIOD_US);

    /* Make sure to disable interrupts lest we get a tick interrupt right as
     * we're adding a new task. */
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        /* For now, we just do an insertion sort so that the _tasks array is
         * always sorted first by priority from highest to lowest and then
         * secondarily by period from lowest to highest.
         *
         * This prioritizes run-time speed over initialization time speed,
         * which is a pretty sane choice for a scheduler. It means that we can
         * always just walk the array of tasks and execute those that are in a
         * ready state.
         * 
         * We could change this to a full re-sort later if we find we want 
         * to change priorities on the fly (not just on insertion) */
        uint8_t insertion_index = _task_count;
        for (uint8_t i = 0; i < _task_count; i++) {
            tasker_task_t *task = &_tasks[i];

            if (task->priority < priority) {
                /* This is where we need to insert */
                insertion_index = i;
                break;
            } else if (task->priority == priority) {
                /* Sort by lowest period having higher priority amongst tasks
                 * that have the same priority. */
                insertion_index = i;
                for (uint8_t j = i; j < _task_count 
                        && _tasks[j].priority == priority; j++) {
                    if (period < _tasks[j].period) {
                        insertion_index = j;
                        break;
                    }
                }
            }
        }

        /* Move tasks down to make room if needed */
        if (_task_count > 0) {
            for (int16_t i = _task_count-1; i >= insertion_index; i--) {
                _tasks[i+1] = _tasks[i];
            }
        }
        
        /* Insert the new task */
        tasker_task_t *new_task = &_tasks[insertion_index];
        new_task->state = IDLE;
        new_task->overruns = 0U;
        new_task->misses = 0U;
        new_task->period = period;
        new_task->phase = phase;
        new_task->priority = priority;
        new_task->fcn = fcn;
        /* NOTE: We start the count at the phase. This means that phased tasks
         * will not become ready on the first tick. */
        new_task->count = phase;
        
        _task_count++;
    } /* End of atomic block */

    return TASKER_E_OK;
}

int8_t Tasker::start(void) {
    if (_state == TASKER_STATE_UNINIT) {
        return TASKER_E_NOT_INITIALIZED;
    }

    if (_clk_src == SRC_TIMER1) {
        /* Set Initial Timer value */
        TCNT1 = 0x0000U;

        /* Clear any pending interrupt */
        TIFR1 = _BV(ICF1);

        /* Interrupt in mode 12 will set the TIRF1 flag, set enable with
         * TIMSK1.ICIE1 */
        TIMSK1 = _BV(ICIE1);
    }

    return TASKER_E_OK;
}

/**
 * ISR - Timer1 Capture Interrupt. This function will go into the vector 
 * table. It executes when TCNT1 matches ICR1 every 100us.
 */
ISR(TIMER1_CAPT_vect) {
    tasker.timerISR();
}

/* The intent is that a single scheduler is instantiated. */
Tasker tasker;

