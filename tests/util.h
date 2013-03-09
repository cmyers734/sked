#ifndef UTIL_H
#define UTIL_H

struct time_array_t {
    uint32_t *tstamps;
    uint8_t count;
    uint8_t size;
};

static bool markTime(time_array_t *times) {
    bool full = false;

    if (times->count < times->size) {
        times->tstamps[times->count++] = micros();
    } else {
        full = true;
    }

    return full;
}

#endif /* #ifndef UTIL_H */
