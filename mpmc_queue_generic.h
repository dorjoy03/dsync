/*
 * Copyright (c) 2024 Dorjoy Chowdhury
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * C implementation of Dmitry Vyukov's "Bounded MPMC Queue" using gcc atomic builtins.
 * Ref: https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
 */

#ifndef MPMC_QUEUE_GENERIC_H
#define MPMC_QUEUE_GENERIC_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define CACHELINE_SIZE 64

#define MPMC_QUEUE_DECLARE(prefix, type)                                          \
                                                                                  \
struct queue_entry {                                                              \
    size_t seq;                                                                   \
    type data;                                                                    \
};                                                                                \
                                                                                  \
/*                                                                                \
 * The paddings are necessary to prevent false cacheline sharing among threads    \
 * which would cause cache coherency traffic among cpu cores.                     \
 */                                                                               \
struct prefix##_mpmc_queue {                                                      \
    uint8_t pad0[CACHELINE_SIZE];                                                 \
    struct queue_entry *queue;                                                    \
    size_t queue_mask;                                                            \
    uint8_t pad1[CACHELINE_SIZE];                                                 \
    size_t enqueue_pos;                                                           \
    uint8_t pad2[CACHELINE_SIZE];                                                 \
    size_t dequeue_pos;                                                           \
    uint8_t pad3[CACHELINE_SIZE];                                                 \
};                                                                                \
                                                                                  \
/*                                                                                \
 * Initialize queue.                                                              \
 *                                                                                \
 * ${queue_length} must be some positive power of two value.                      \
 * Returns a valid "handle" to be used in enqueue, dequeue, free functions.       \
 * Otherwise, returns NULL on error.                                              \
 */                                                                               \
struct prefix##_mpmc_queue *                                                      \
prefix##_mpmc_queue_init(size_t queue_length)                                     \
{                                                                                 \
    assert(queue_length >= 2 && (queue_length & (queue_length - 1)) == 0);        \
    assert(queue_length <= SIZE_MAX / sizeof(struct queue_entry));                \
                                                                                  \
    struct prefix##_mpmc_queue *Q = malloc(sizeof(struct prefix##_mpmc_queue));   \
    if (Q == NULL)                                                                \
        goto err0;                                                                \
                                                                                  \
    size_t size = queue_length * sizeof(struct queue_entry);                      \
    struct queue_entry *queue = malloc(size);                                     \
    if (queue == NULL)                                                            \
        goto err1;                                                                \
                                                                                  \
    Q->queue = queue;                                                             \
    Q->queue_mask = queue_length - 1;                                             \
                                                                                  \
    for (size_t i = 0; i < queue_length; ++i) {                                   \
        __atomic_store_n(&Q->queue[i].seq, i, __ATOMIC_RELAXED);                  \
    }                                                                             \
    __atomic_store_n(&Q->enqueue_pos, 0, __ATOMIC_RELAXED);                       \
    __atomic_store_n(&Q->dequeue_pos, 0, __ATOMIC_RELAXED);                       \
                                                                                  \
    return Q;                                                                     \
                                                                                  \
 err1:                                                                            \
    free(Q);                                                                      \
                                                                                  \
 err0:                                                                            \
    return NULL;                                                                  \
}                                                                                 \
                                                                                  \
/*                                                                                \
 * Free queue.                                                                    \
 */                                                                               \
void                                                                              \
prefix##_mpmc_queue_free(struct prefix##_mpmc_queue *Q)                           \
{                                                                                 \
    if (Q != NULL) {                                                              \
        free(Q->queue);                                                           \
        free(Q);                                                                  \
    }                                                                             \
    return;                                                                       \
}                                                                                 \
                                                                                  \
/*                                                                                \
 * Enqueue ${data}.                                                               \
 *                                                                                \
 * Returns 0 if ${data} is successfully enqueued. Otherwise, returns -1 when queue\
 * is full.                                                                       \
 */                                                                               \
int                                                                               \
prefix##_mpmc_queue_enqueue(struct prefix##_mpmc_queue *Q, type data)             \
{                                                                                 \
    struct queue_entry *entry;                                                    \
    size_t mask = Q->queue_mask;                                                  \
    size_t pos = __atomic_load_n(&Q->enqueue_pos, __ATOMIC_RELAXED);              \
                                                                                  \
    while(true) {                                                                 \
        entry = &Q->queue[pos & mask];                                            \
        size_t seq = __atomic_load_n(&entry->seq, __ATOMIC_ACQUIRE);              \
                                                                                  \
        if (seq == pos) {                                                         \
            if (__atomic_compare_exchange_n(&Q->enqueue_pos, &pos, pos + 1, true, \
                                            __ATOMIC_RELAXED, __ATOMIC_RELAXED))  \
                break;                                                            \
        } else if (seq < pos) {                                                   \
            return -1;                                                            \
        } else {                                                                  \
            pos = __atomic_load_n(&Q->enqueue_pos, __ATOMIC_RELAXED);             \
        }                                                                         \
    }                                                                             \
                                                                                  \
    entry->data = data;                                                           \
    __atomic_store_n(&entry->seq, pos + 1, __ATOMIC_RELEASE);                     \
                                                                                  \
    return 0;                                                                     \
 }                                                                                \
                                                                                  \
/*                                                                                \
 * Dequeue next queue entry into ${*data}.                                        \
 *                                                                                \
 * Returns 0 when an entry is successfully dequeued. Otherwise, returns -1 when   \
 * queue is empty.                                                                \
 */                                                                               \
int                                                                               \
prefix##_mpmc_queue_dequeue(struct prefix##_mpmc_queue *Q, type *data)            \
{                                                                                 \
    struct queue_entry *entry;                                                    \
    size_t mask = Q->queue_mask;                                                  \
    size_t pos = __atomic_load_n(&Q->dequeue_pos, __ATOMIC_RELAXED);              \
                                                                                  \
    while(true) {                                                                 \
        entry = &Q->queue[pos & mask];                                            \
        size_t seq = __atomic_load_n(&entry->seq, __ATOMIC_ACQUIRE);              \
                                                                                  \
        if (seq == (size_t)(pos + 1)) {                                           \
            if (__atomic_compare_exchange_n(&Q->dequeue_pos, &pos, pos + 1, true, \
                                            __ATOMIC_RELAXED, __ATOMIC_RELAXED))  \
                break;                                                            \
        } else if (seq < (size_t)(pos + 1)) {                                     \
            return -1;                                                            \
        } else {                                                                  \
            pos = __atomic_load_n(&Q->dequeue_pos, __ATOMIC_RELAXED);             \
        }                                                                         \
    }                                                                             \
                                                                                  \
    *data = entry->data;                                                          \
    __atomic_store_n(&entry->seq, pos + mask + 1, __ATOMIC_RELEASE);              \
                                                                                  \
    return 0;                                                                     \
}

#endif /* MPMC_QUEUE_GENERIC_H */
