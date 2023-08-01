#include "cbuf.h"
#include <stdio.h>

/* Initialize the circular buffer
 * Arguments:
 *   circular_buffer *cb - reference to the circular buffer
 *   size_t capacity     - maximum capacity of the circular buffer
 * Return Value:
 *   CB_SUCCESS on success
 *   CB_MEMORY_ERROR on error
 */
int cb_init(circular_buffer *cb, size_t capacity){
    cb->buffer = malloc(capacity);
    if (cb->buffer == NULL){
        return CB_MEMORY_ERROR;
    }

    cb->sidx = 0;
    cb->eidx = 0;
    cb->full = 0;
    cb->max_cap = capacity;

    return CB_SUCCESS;
}

/* Get current free capacity of the circular buffer
 * Arguments:
 *   circular_buffer *cb - reference to the circular buffer
 * Return Value:
 *   Free capacity (in bytes)
 */
long cb_free_cp(circular_buffer *cb){
    long free_cp = -1;

    // Calculate distance from ending index to starting index
    if (cb->sidx > cb->eidx){ // Distance is contiguous
        free_cp = cb->sidx - cb->eidx;
    }
    else if (cb->sidx < cb->eidx){ // Distance wraps around end of buffer
        free_cp = cb->max_cap - (cb->eidx - cb->sidx);
    }
    else{ // Starting and ending indices are equal
        if (cb->full){
            free_cp = 0; // No free capacity
        }
        else{
            free_cp = cb->max_cap; // Entire buffer is free
        }
    }

    return free_cp;
}

/* Add data onto the end of the circular buffer
 * Arguments:
 *   circular_buffer *cb - reference to the circular buffer
 *   const void *buf     - reference to the data source buffer
 *   unsigned int in_sz  - size (in bytes) of data source buffer
 * Return Value:
 *   CB_SUCCESS on success
 *   CB_OVERFLOW_ERROR on error
 */
int cb_push_back(circular_buffer *cb, const void *buf, unsigned int in_sz){
    size_t free_cp = cb_free_cp(cb);
    if (in_sz > free_cp){ // Not enough free capacity to add the new data
        return CB_OVERFLOW_ERROR;
    }

    if (cb->sidx <= cb->eidx){ // Added data may not be continguous in circular buffer
        size_t tail_cp = cb->max_cap - cb->eidx;
        if (in_sz <= tail_cp){ // Added data is continguous in circular buffer
            memcpy(cb->buffer + cb->eidx, buf, in_sz);
            cb->eidx += in_sz;
            if (cb->eidx == cb->max_cap){ // Edge case to wrap ending index back to 0
                cb->eidx = 0;
            }
        }
        else{ // Added data wraps around end of circular buffer
            memcpy(cb->buffer + cb->eidx, buf, tail_cp);
            memcpy(cb->buffer, buf + tail_cp, in_sz - tail_cp);
            cb->eidx = in_sz - tail_cp;
        }
    }
    else{  // Added data is continguous in circular buffer
        memcpy(cb->buffer + cb->eidx, buf, in_sz);
        cb->eidx += in_sz;
    }

    if(free_cp == in_sz){ // Check if circular buffer is now full
        cb->full = 1;
    }

    return CB_SUCCESS;
}

/* Remove data from the beginning of the circular buffer
 * Arguments:
 *   circular_buffer *cb - reference to the circular buffer
 *   const void *buf     - reference to the buffer to store popped data
 *   unsigned int max_sz  - size (in bytes) of maximum amount of data to be popped
 * Return Value:
 *   Data popped (in bytes) on success
 *   -1 on error
 */
long cb_pop_front(circular_buffer *cb, void *buf, unsigned int max_sz){
    long osz = -1;
    // No data in circular buffer or no available space in buffer to store popped data
    if(cb_free_cp(cb) == cb->max_cap || max_sz == 0){
        return osz;
    }

    if (cb->sidx < cb->eidx){ // Popped data is contiguous in circular buffer
        osz = MIN(max_sz, cb->max_cap - cb_free_cp(cb));
        memcpy(buf, cb->buffer + cb->sidx, osz);
        cb->sidx += osz;
    }
    else{ // Popped data may not be contiguous in circular buffer
        osz = MIN(max_sz, cb->max_cap - cb->sidx);
        memcpy(buf, cb->buffer + cb->sidx, osz);
        if (osz < max_sz){ // Popped data is not contiguous in circular buffer
            memcpy(buf+osz, cb->buffer, MIN(max_sz-osz, cb->eidx));
            cb->sidx = MIN(max_sz-osz, cb->eidx);
            osz += MIN(max_sz-osz, cb->eidx);
        }
        else{ // Popped data is contiguous in circular buffer
            cb->sidx += osz;
            if (cb->sidx == cb->max_cap){ // Edge case to wrap starting index back to 0
                cb->sidx = 0;
            }
        }
        cb->full=0; // Circular buffer no longer full since data must have been popped
    }

    return osz;
}

/* Print debug information about the circular buffer
 * Arguments:
 *   circular_buffer *cb - reference to the circular buffer
 * Return Value:
 *   None
 */
void print_cb_status(circular_buffer *cb){
    printf("sidx\t eidx\t full\t maxcap\n");
    printf("%-4lu\t %-4lu\t %-4d\t %-6lu\n\n", \
           cb->sidx, cb->eidx, cb->full, cb->max_cap);
    return;
}
