#ifndef TIPC_H
#define TIPC_H

#include "tipc_core.h"

/* Kernel IPC for M6b: bounded mailboxes with blocking send/recv and a
 * recursive mutex, built on the M6a scheduler. All the tricky data
 * structures live in tipc_core.h as pure helpers (host-tested by
 * `make smoke`); tipc.c wires them to ttask_block()/ttask_wake() and
 * keeps a critical section around every primitive.
 *
 * Single-CPU kernel: mutual exclusion is provided by cli/sti sections, so
 * these are safe against preemption but not against inter-CPU races (there
 * is only one CPU). */

/* Create/free a mailbox. Freeing is the caller's tfree(); it is only safe
 * once every sender/receiver has exited. */
int tipc_mailbox_init(tipc_mailbox_t *mb);

/* Send a message. Blocks the calling task until a slot is free. msg->sender
 * is stamped with the calling task id. Returns 0 on delivery, -1 on invalid
 * args or waiter-list exhaustion. */
int tipc_send(tipc_mailbox_t *mb, tipc_msg_t *msg);

/* Receive a message. Blocks the calling task until one arrives. */
int tipc_recv(tipc_mailbox_t *mb, tipc_msg_t *out);

/* Recursive mutex. A task that holds it may lock it again (depth++); every
 * lock must be matched by an unlock. Deadlock is not detected. */
int tipc_mutex_init(tipc_mutex_t *m);
int tipc_mutex_lock(tipc_mutex_t *m);
int tipc_mutex_unlock(tipc_mutex_t *m);

#endif
