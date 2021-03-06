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
 *---------------------------------------------------------------------------
 *
 * This library provides both preemptive and non-preemptive task execution 
 * for the Uno Arduino platform, although it's possible that this works just 
 * fine on other boards.
 *
 * This scheduler is:
 *   - Single stack: that means that if tasks preempt one another, the stack 
 *     just keeps getting deeper. This isn't usually a problem with simpler 
 *     systems like Arduino... it'll be problematic only in degenerate cases.
 *     Contrast with schedulers/OSes that have a stack allocated per task.
 *     Before task switching, the stack pointer is moved to that task's private
 *     stack. It's just more to worry about but has its advantages.
 *   - Option 1 - Preemptive: that means one task can interrupt another and 
 *     start executing. This can be really useful! Imagine you have a task 
 *     that you need to execute every 1s, but have to sample an analog pin 
 *     every 10ms.You want the 10ms task to preempt the 1s task and return 
 *     when it's done.
 *   - Option 2 - Non-Preemptive: If you prefer, you can avoid the additional
 *     rigor that comes with programming in a preemptive tasking environment
 *     and operate Sked in a non-preemptive mode.
 *
 *
 * Typical Usage Flow:
 *  1. Call init()
 *  2. Call schedule() x n
 *  3. Call start()
 */

#include <util/atomic.h>
#include "./Sked.h"

/* Fixed 100us tick resolution */
#define SKED_TIMER1_CLK_HZ (8.0/static_cast<float>(F_CPU))
#define SKED_TIMER1_TICK_PERIOD_S 0.000100
#define SKED_TIMER1_TICK_PERIOD_US (SKED_TIMER1_TICK_PERIOD_S * 1000000.0)
/* The number of ticks needed for TIMER1 to expire at the desired period */
#define SKED_TIMER1_TICKS_PER_PERIOD (SKED_TIMER1_TICK_PERIOD_S \
    / SKED_TIMER1_CLK_HZ)

/* Different states the sked can be in */
#define SKED_STATE_UNINIT 0
#define SKED_STATE_INIT 1

/**
 * Constructor
 */
Sked::Sked(void) {
    reset();
}

/**
 * Initialize Sked. Must be called before any other function as it uses the
 * timer that you want to use and the mode you want to operate in and uses
 * those to define how it behaves going forward.
 *
 * @param mode  Must be either the preemptive or non-preemptive mode
 * @param clk_src  This tells Sked which timer to use. Currently, only TIMER1
 * is supported (SKED_SRC_TIMER1).
 *
 * @return SKED_E_OK - The initialization operation was successful.
 *         SKED_E_NOT_IMPLEMENTED - if a timer source other than SKED_SRC_TIMER1
 *         is used.
 */
int8_t Sked::init(sked_mode_e mode, sked_clk_src_e clk_src) {
    int8_t ret = SKED_E_OK;

    _mode = mode;
    _clk_src = clk_src;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (_clk_src == SKED_SRC_TIMER1) {
            /* Maximum number of us that can fit in 16-bit counter */
            _max_period_us = (uint32_t)(static_cast<float>(0xFFFF)
                    * static_cast<float>(SKED_TIMER1_TICK_PERIOD_US));
            _min_period_us = SKED_TIMER1_TICK_PERIOD_US;

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
             * periods and offsets need to be even multiples of 100us. */
            ICR1 = (uint16_t)(SKED_TIMER1_TICKS_PER_PERIOD-1);

            /* Interrupt in mode 12 will set the TIRF1 flag, leave disabled
             * until the call to start() */
            TIMSK1 = 0x00;

            _state = SKED_STATE_INIT;
        } else {
            ret = SKED_E_NOT_IMPLEMENTED;
        }
    }

    return ret;
}

/**
 * When operating Sked in a non-preemptive mode, this function will walk
 * through the list of tasks in priority order looking for tasks that are
 * ready to run. When it finds one, it changes its state and runs it.
 *
 * If you are using Sked in a preemptive mode, this function will do nothing
 * and simply return SKED_E_WRONG_MODE.
 *
 * @return SKED_E_OK - The loop ran successfully (may not have run any tasks)
 *         SKED_E_NOT_INITIALIZED - You have not called init()
 *         SKED_E_WRONG_MODE - Sked is operating in preemptive mode
 */
int8_t Sked::loop(void) {
    if (_state == SKED_STATE_UNINIT) {
        return SKED_E_NOT_INITIALIZED;
    }

    if (_mode == SKED_MODE_NON_PREEMPTIVE) {
        /* Search for a task that is ready to run and execute it */
        for (uint8_t i = 0; i < _task_count; i++) {
            sked_task_t *task = &_tasks[i];

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
    } else {
        return SKED_E_WRONG_MODE;
    }

    return SKED_E_OK;
}

/**
 * This ISR is connected to the timer overflow such that it runs at a regular
 * rate and performs internal bookkeeping tasks.
 */
void Sked::timerISR(void) {
    /* This occurs periodically. Walk through each task and update its state. */
    for (uint8_t i = 0; i < _task_count; i++) {
        sked_task_t *task = &_tasks[i];

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
                task->overruns = constrain(SKED_OVERRUNS_MAX,
                        0, task->overruns+1);
            } else {
                /* Miss! */
                task->misses = constrain(SKED_MISSES_MAX,
                        0, task->misses+1);
            }

            /* Reset the count back to the period. You'll note that we don't
             * care about the offset. Since that was baked in when the task was
             * scheduled, it meant that its first period was offset
             * differently and thus this task will continue to have the
             * offset baked into each subsequent period without worrying about
             * it. */
            task->count = task->period;
        }
    }

    if (_mode == SKED_MODE_PREEMPTIVE) {
        /* After we update our state, it's time to execute an available task */
        for (uint8_t i = 0; i < _task_count; i++) {
            sked_task_t *task = &_tasks[i];

            /* We can run a task when it's ready and is of higher priority than
             * the task we're running already. */
            if ((task->state == READY)
                    && (task->priority > _current_task_priority)) {
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
                _current_task_priority = SKED_MIN_PRIORITY;

                task->state = IDLE;
            }
        }
    }
}

#if (SKED_DEBUG == SKED_ON)
/**
 * A debugging routine that prints out the state of Sked to the given stream.
 *
 * @param stream  A pointer to a print stream. The simplest thing to do is
 * call it with &Serial.
 */
void Sked::debugPrintState(Stream *stream) {
    if (_state == SKED_STATE_UNINIT) {
        stream->println("### Sked is UNINITIALIZED.");
    } else {
        stream->println("### Sked is INITIALIZED.");
        stream->print("### Max Period (us): ");
        stream->println(_max_period_us);
        stream->print("### Src Timer:       ");
        switch (_clk_src) {
            case SKED_SRC_TIMER1:
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
                stream->println(SKED_TIMER1_TICKS_PER_PERIOD);
                break;

            default:
                stream->println("INVALID");
                break;
        }

        stream->print("### Tasks: "); stream->println(_task_count);
        for (uint8_t i = 0; i < _task_count; i++) {
            sked_task_t *task = &_tasks[i];

            stream->print("###   Task[");
            stream->print(i);
            stream->print("]: ");
            stream->print("(");
            stream->print(task->priority);
            stream->print(", ");
            stream->print(task->period);
            stream->print(", ");
            stream->print(task->offset);
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
#endif /* #if (SKED_DEBUG == SKED_ON) */

/**
 * Schedules a routine to be called at a given period, offset, and priority.
 *
 * @param period_us The period (time between successive calls) of the task
 * in microseconds. It must be greater than 0, but less than the maximum
 * and greater than the minimum (which are timer and platform dependent).
 * On the Uno, the max is ~65.5 seconds and the minimum is 100 us.
 * with TIMER1.
 * @param offset_us The offset (time to wait before executing the first task)
 * in microseconds. You can use this to make it so two tasks won't occur at
 * the same time even though they have the same period. The period may be 0,
 * but it if it's greather than 0, it should be greater than the minimum
 * period.
 * @param priority  This is an indicator of task priority. The higher this
 * value, the higher the priority of the task. If there are two tasks that
 * could run at the same time, the one with the higher priority is chosen. The
 * minimum valid priority is SKED_MIN_PRIORITY (-128 is reserved).
 * @param fcn  A pointer to a void (void) function (sked_task_fcn_t). Should
 * not be null.
 *
 * @return SKED_E_OK - The task was scheduled successfully
 *         SKED_E_NOT_INITIALIZED - you should call init() first
 *         SKED_E_TOO_MANY_TASKS - you can't have too many tasks (> SKED_MAX_TASKS)
 *         SKED_E_INVALID_PERIOD - The period isn't valid
 *         SKED_E_INVALID_OFFSET - The offset isn't valid
 *         SKED_E_INVALID_PRIORITY - The priority is not valid
 *         SKED_E_INVALID_FUNCTION - Your function pointer was null
 */
int8_t Sked::schedule(uint32_t period_us, uint32_t offset_us, int8_t priority,
        sked_task_fcn_t fcn) {
    uint16_t period;
    uint16_t offset;

    /* Have to initialize first to get proper bounds */
    if (_state == SKED_STATE_UNINIT) {
        return SKED_E_NOT_INITIALIZED;
    }

    /* We may be full */
    if (_task_count >= SKED_MAX_TASKS) {
        return SKED_E_TOO_MANY_TASKS;
    }

    /* Check period and offset:
     *  Both must be less than the max and greater than the min (except for
     *  the offset, which can be 0, but if it's greater than 0, it should be
     *  greater than 100).
     */
    if (period_us > _max_period_us || period_us < _min_period_us) {
        return SKED_E_INVALID_PERIOD;
    }

    if (offset_us > _max_period_us
            || (offset_us > 0 && offset_us < _min_period_us)) {
        return SKED_E_INVALID_OFFSET;
    }

    /* Priority needs to be > -127 (which we use to designate the lowest
     * possible priority)
     */
    if (priority <= (int8_t)SKED_MIN_PRIORITY) {
        return SKED_E_INVALID_PRIORITY;
    }

    /* Task function cannot be NULL */
    if (fcn == (sked_task_fcn_t)NULL) {
        return SKED_E_INVALID_FUNCTION;
    }

    /* Convert period and offset to ticks of the timer ISR */
    period = period_us / (uint32_t)(SKED_TIMER1_TICK_PERIOD_US);
    offset = offset_us / (uint32_t)(SKED_TIMER1_TICK_PERIOD_US);

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
            sked_task_t *task = &_tasks[i];

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
        sked_task_t *new_task = &_tasks[insertion_index];
        new_task->state = IDLE;
        new_task->overruns = 0U;
        new_task->misses = 0U;
        new_task->period = period;
        new_task->offset = offset;
        new_task->priority = priority;
        new_task->fcn = fcn;
        /* NOTE: We start the count at the offset. This means that offset tasks
         * will not become ready on the first tick. */
        new_task->count = offset;

        _task_count++;
    } /* End of atomic block */

    return SKED_E_OK;
}

/**
 * Provides you with the current task count. Will return 0 out of reset or
 * after a call to reset().
 *
 * @return The number of tasks [0, SKED_MAX_TASKS]
 */
uint8_t Sked::getTaskCount(void) {
    return _task_count;
}

/**
 * This started out being just for testing, but provides you with access to
 * the underlying task structure. Don't make any changes unless you know what
 * you're doing.
 *
 * Keep in mind that when new tasks are added, they're insertion sorted. That
 * means higher indexes in the table mean lower priority tasks.
 *
 * @param task_index The index of the task you want [0, getTaskCount())
 *
 * @return A pointer to the task structure at that index or NULL if there is
 * no such task
 */
sked_task_t *Sked::getTaskInfo(uint8_t task_index) {
    if (task_index < _task_count) {
        return &_tasks[task_index];
    } else {
        return NULL;
    }
}

/**
 * Started out being used for testing, but included because it could be
 * useful. This resets the internal structure
 */
void Sked::reset(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        /* Don't bother to initialize any of the task slots because _task_count
         * determines which are valid. */
        _task_count = 0U;
        _max_period_us = 0U;
        _min_period_us = 0U;
        _state = SKED_STATE_UNINIT;
        _current_task_priority = SKED_MIN_PRIORITY;
        _mode = SKED_MODE_PREEMPTIVE;

        if (_clk_src == SKED_SRC_TIMER1) {
            /* Set Initial Timer value */
            TCNT1 = 0x0000U;

            /* Interrupt in mode 12 will set the TIRF1 flag, disable with
             * clear of TIMSK1.ICIE1 (just clear them all for now). */
            TIMSK1 = 0x00U;

            /* Clear any pending interrupt */
            TIFR1 = _BV(ICF1);
        }
    } /* ATOMIC_BLOCK(ATOMIC_RESTORESTATE) */
}

/**
 * Call this after init() and scheduling tasks to actually start executing
 * tasks.
 *
 * @return SKED_E_OK - Sked was successfully started
 *         SKED_E_NOT_INITIALIZED - You have not called init()
 */
int8_t Sked::start(void) {
    if (_state == SKED_STATE_UNINIT) {
        return SKED_E_NOT_INITIALIZED;
    }

    if (_clk_src == SKED_SRC_TIMER1) {
        /* Set Initial Timer value */
        TCNT1 = 0x0000U;

        /* Clear any pending interrupt */
        TIFR1 = _BV(ICF1);

        /* Interrupt in mode 12 will set the TIRF1 flag, set enable with
         * TIMSK1.ICIE1 */
        TIMSK1 = _BV(ICIE1);
    }

    return SKED_E_OK;
}

/**
 * ISR - Timer1 Capture Interrupt. This function will go into the vector
 * table. It executes when TCNT1 matches ICR1 every 100us.
 */
ISR(TIMER1_CAPT_vect) {
    sked.timerISR();
}

/* The intent is that a single scheduler is instantiated. */
Sked sked;

