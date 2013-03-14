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
 */

#ifndef TESTS_UTIL_H_
#define TESTS_UTIL_H_

struct time_array_t {
    uint32_t *tstamps;
    uint8_t count;
    uint8_t size;
};

/**
 * Utility to help record execution times. Call to add another time to the
 * array and return true if the array is full.
 */
static bool markTime(time_array_t *times) {
    bool full = false;

    if (times->count < times->size) {
        times->tstamps[times->count++] = micros();
    } else {
        full = true;
    }

    return full;
}

#endif  // TESTS_UTIL_H_

