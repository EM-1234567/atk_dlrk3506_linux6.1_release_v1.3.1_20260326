/**
 * @file tuya_bluez_compat.h
 * @brief Portable shims replacing SDK-internal uni_log / mem_pool / uni_queue
 *        for the BlueZ stack in tuyaos_adapter.
 * @version 1.0
 * @date 2026-07-21
 * @copyright Copyright (c) Tuya Inc.
 */
#ifndef __TUYA_BLUEZ_COMPAT_H__
#define __TUYA_BLUEZ_COMPAT_H__

#include "tkl_memory.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Logging (replaces uni_log.h)
 * Dual output: stdout (→ wukong_ai.log) and /var/log/ble_diag.log for BLE debug.
 * --------------------------------------------------------------------------- */
#ifndef BLE_DIAG_LOG_PATH
#define BLE_DIAG_LOG_PATH "/var/log/ble_diag.log"
#endif

static inline void __ble_log_write(const char *level, const char *fmt, ...)
{
    va_list ap;
    FILE *fp = NULL;
    struct timespec ts;
    long ms = 0;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ms = (long)(ts.tv_nsec / 1000000L);

    va_start(ap, fmt);
    printf("[ble][%s] ", level);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);

    fp = fopen(BLE_DIAG_LOG_PATH, "a");
    if (fp != NULL) {
        va_start(ap, fmt);
        fprintf(fp, "[%ld.%03ld][ble][%s] ", (long)ts.tv_sec, ms, level);
        vfprintf(fp, fmt, ap);
        fprintf(fp, "\n");
        va_end(ap);
        fclose(fp);
    }
}

#ifndef PR_DEBUG
#define PR_DEBUG(fmt, ...) do { __ble_log_write("D", fmt, ##__VA_ARGS__); } while (0)
#endif
#ifndef PR_INFO
#define PR_INFO(fmt, ...)  do { __ble_log_write("I", fmt, ##__VA_ARGS__); } while (0)
#endif
#ifndef PR_WARN
#define PR_WARN(fmt, ...)  do { __ble_log_write("W", fmt, ##__VA_ARGS__); } while (0)
#endif
#ifndef PR_ERR
#define PR_ERR(fmt, ...)   do { __ble_log_write("E", fmt, ##__VA_ARGS__); } while (0)
#endif

/* ---------------------------------------------------------------------------
 * Memory (replaces mem_pool.h Malloc/Free)
 * --------------------------------------------------------------------------- */
#ifndef Malloc
#define Malloc(sz) tkl_system_malloc(sz)
#endif
#ifndef Free
#define Free(p)    do { if (p) { tkl_system_free(p); } } while (0)
#endif
#ifndef SIZEOF
#define SIZEOF sizeof
#endif

/* ---------------------------------------------------------------------------
 * Simple pointer queue (replaces uni_queue.h for BLE write-cache only)
 * --------------------------------------------------------------------------- */
#define BLE_CACHE_Q_DEPTH 32

typedef struct {
    void *slots[BLE_CACHE_Q_DEPTH];
    int head;
    int tail;
    int count;
} BLE_PTR_QUEUE_T;

typedef BLE_PTR_QUEUE_T *P_QUEUE_CLASS;

/**
 * @brief Create a pointer queue used by BLE write-cache
 * @param[in] depth ignored (fixed to BLE_CACHE_Q_DEPTH)
 * @param[in] elem_size ignored (stores void *)
 * @return queue handle, or NULL on failure
 */
static inline P_QUEUE_CLASS CreateQueueObj(int depth, int elem_size)
{
    P_QUEUE_CLASS q;
    (void)depth;
    (void)elem_size;
    q = (P_QUEUE_CLASS)tkl_system_malloc(sizeof(*q));
    if (q == NULL) {
        return NULL;
    }
    memset(q, 0, sizeof(*q));
    return q;
}

/**
 * @brief Enqueue one pointer
 * @param[in] q queue
 * @param[in] in address of the pointer to store
 * @param[in] n must be 1
 * @return 1 on success, 0 on failure
 */
static inline int InQueue(P_QUEUE_CLASS q, unsigned char *in, int n)
{
    void *ptr;
    if ((q == NULL) || (in == NULL) || (n != 1)) {
        return 0;
    }
    if (q->count >= BLE_CACHE_Q_DEPTH) {
        return 0;
    }
    memcpy(&ptr, in, sizeof(ptr));
    q->slots[q->tail] = ptr;
    q->tail = (q->tail + 1) % BLE_CACHE_Q_DEPTH;
    q->count++;
    return 1;
}

/**
 * @brief Dequeue one pointer
 * @param[in] q queue
 * @param[out] out address of pointer to receive
 * @param[in] n must be 1
 * @return 1 on success, 0 on failure
 */
static inline int OutQueue(P_QUEUE_CLASS q, unsigned char *out, int n)
{
    void *ptr;
    if ((q == NULL) || (out == NULL) || (n != 1) || (q->count <= 0)) {
        return 0;
    }
    ptr = q->slots[q->head];
    q->slots[q->head] = NULL;
    q->head = (q->head + 1) % BLE_CACHE_Q_DEPTH;
    q->count--;
    memcpy(out, &ptr, sizeof(ptr));
    return 1;
}

/**
 * @brief Get current queue depth
 * @param[in] q queue
 * @return number of queued items
 */
static inline int GetCurQueNum(P_QUEUE_CLASS q)
{
    if (q == NULL) {
        return 0;
    }
    return q->count;
}

#ifdef __cplusplus
}
#endif

#endif /* __TUYA_BLUEZ_COMPAT_H__ */
