#ifndef CIRCULAR_BUF_H
#define CIRCULAR_BUF_H

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define CB_SUCCESS           0  /* Circular buffer operation was successful */
#define CB_MEMORY_ERROR      1  /* Failed to allocate memory */
#define CB_OVERFLOW_ERROR    2  /* Circular buffer is full - cannot push more items */
#define CB_EMPTY_ERROR       3  /* Circular buffer is empty - cannot pop more items */

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef struct circular_buffer {
    void *buffer;         // Pointer to beginning of the allocated buffer
    size_t sidx;          // Starting index of the circular buffer
    size_t eidx;          // Ending index of the circular buffer (Note: no valid data at this index)
    size_t max_cap;       // Maximum capacity of the circular buffer
    unsigned char full;   // Whether the circular buffer is full (Note: "sidx == eidx" would otherwise be ambiguous)
} circular_buffer;

int cb_init(circular_buffer *cb, size_t capacity);
long cb_free_cp(circular_buffer *cb);
int cb_push_back(circular_buffer *cb, const void *buf, unsigned int in_sz);
long cb_pop_front(circular_buffer *cb, void *buf, unsigned int max_sz);
void print_cb_status(circular_buffer *cb);

#endif
