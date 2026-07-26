#include <os/lock.h>
#include <os/sched.h>
#include <os/list.h>
#include <os/string.h>
#include <atomic.h>

mutex_lock_t mlocks[LOCK_NUM];
barrier_t barriers[BARRIER_NUM];
condition_t conditions[CONDITION_NUM];
semaphore_t semaphores[SEMAPHORE_NUM];
mailbox_t mboxes[MBOX_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++) {
        spin_lock_init(&mlocks[i].lock);
        init_list_head(&mlocks[i].block_queue);
        mlocks[i].key = -1;
    }

    init_barriers();
    init_conditions();
    init_semaphores();
    init_mbox();
}

void spin_lock_init(spin_lock_t *lock)
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    return atomic_swap(LOCKED, (ptr_t)&lock->status) == UNLOCKED;
}

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while (!spin_lock_try_acquire(lock)) {
        ;
    }
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    atomic_swap(UNLOCKED, (ptr_t)&lock->status);
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    for (int i = 0; i < LOCK_NUM; i++) {
        if (mlocks[i].key == key) {
            return i;
        }
    }

    for (int i = 0; i < LOCK_NUM; i++) {
        if (mlocks[i].key == -1) {
            mlocks[i].key = key;
            spin_lock_init(&mlocks[i].lock);
            init_list_head(&mlocks[i].block_queue);
            return i;
        }
    }

    return -1;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    mutex_lock_t *lock = &mlocks[mlock_idx];
    while (!spin_lock_try_acquire(&lock->lock)) {
        do_block(&current_running->list, &lock->block_queue);
    }
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    mutex_lock_t *lock = &mlocks[mlock_idx];
    spin_lock_release(&lock->lock);

    if (!list_empty(&lock->block_queue)) {
        do_unblock(lock->block_queue.next);
    }
}

void init_barriers(void)
{
    for (int i = 0; i < BARRIER_NUM; i++) {
        barriers[i].key = -1;
        barriers[i].goal = 0;
        barriers[i].count = 0;
        spin_lock_init(&barriers[i].lock);
        init_list_head(&barriers[i].block_queue);
    }
}

int do_barrier_init(int key, int goal)
{
    if (goal <= 0)
        return -1;

    for (int i = 0; i < BARRIER_NUM; i++) {
        if (barriers[i].key == key)
            return i;
    }

    for (int i = 0; i < BARRIER_NUM; i++) {
        if (barriers[i].key == -1) {
            barriers[i].key = key;
            barriers[i].goal = goal;
            barriers[i].count = 0;
            spin_lock_init(&barriers[i].lock);
            init_list_head(&barriers[i].block_queue);
            return i;
        }
    }
    return -1;
}

void do_barrier_wait(int bar_idx)
{
    if (bar_idx < 0 || bar_idx >= BARRIER_NUM || barriers[bar_idx].key == -1)
        return;

    barrier_t *bar = &barriers[bar_idx];
    spin_lock_acquire(&bar->lock);

    if (bar->count + 1 < bar->goal) {
        bar->count++;
        spin_lock_release(&bar->lock);
        do_block(&current_running->list, &bar->block_queue);
    } else {
        bar->count = 0;
        while (!list_empty(&bar->block_queue)) {
            do_unblock(bar->block_queue.next);
        }
        spin_lock_release(&bar->lock);
    }
}

void do_barrier_destroy(int bar_idx)
{
    if (bar_idx < 0 || bar_idx >= BARRIER_NUM || barriers[bar_idx].key == -1)
        return;

    barrier_t *bar = &barriers[bar_idx];
    spin_lock_acquire(&bar->lock);
    while (!list_empty(&bar->block_queue)) {
        do_unblock(bar->block_queue.next);
    }
    bar->key = -1;
    bar->goal = 0;
    bar->count = 0;
    spin_lock_release(&bar->lock);
}

void init_conditions(void)
{
    for (int i = 0; i < CONDITION_NUM; i++) {
        conditions[i].key = -1;
        spin_lock_init(&conditions[i].lock);
        init_list_head(&conditions[i].block_queue);
    }
}

int do_condition_init(int key)
{
    for (int i = 0; i < CONDITION_NUM; i++) {
        if (conditions[i].key == key)
            return i;
    }
    for (int i = 0; i < CONDITION_NUM; i++) {
        if (conditions[i].key == -1) {
            conditions[i].key = key;
            spin_lock_init(&conditions[i].lock);
            init_list_head(&conditions[i].block_queue);
            return i;
        }
    }
    return -1;
}

void do_condition_wait(int cond_idx, int mutex_idx)
{
    if (cond_idx < 0 || cond_idx >= CONDITION_NUM || conditions[cond_idx].key == -1)
        return;

    do_mutex_lock_release(mutex_idx);
    do_block(&current_running->list, &conditions[cond_idx].block_queue);
    do_mutex_lock_acquire(mutex_idx);
}

void do_condition_signal(int cond_idx)
{
    if (cond_idx < 0 || cond_idx >= CONDITION_NUM || conditions[cond_idx].key == -1)
        return;

    if (!list_empty(&conditions[cond_idx].block_queue))
        do_unblock(conditions[cond_idx].block_queue.next);
}

void do_condition_broadcast(int cond_idx)
{
    if (cond_idx < 0 || cond_idx >= CONDITION_NUM || conditions[cond_idx].key == -1)
        return;

    while (!list_empty(&conditions[cond_idx].block_queue)) {
        do_unblock(conditions[cond_idx].block_queue.next);
    }
}

void do_condition_destroy(int cond_idx)
{
    if (cond_idx < 0 || cond_idx >= CONDITION_NUM || conditions[cond_idx].key == -1)
        return;

    do_condition_broadcast(cond_idx);
    conditions[cond_idx].key = -1;
}

void init_semaphores(void)
{
    for (int i = 0; i < SEMAPHORE_NUM; i++) {
        semaphores[i].key = -1;
        semaphores[i].val = 0;
        spin_lock_init(&semaphores[i].lock);
        init_list_head(&semaphores[i].block_queue);
    }
}

int do_semaphore_init(int key, int init)
{
    for (int i = 0; i < SEMAPHORE_NUM; i++) {
        if (semaphores[i].key == key)
            return i;
    }
    for (int i = 0; i < SEMAPHORE_NUM; i++) {
        if (semaphores[i].key == -1) {
            semaphores[i].key = key;
            semaphores[i].val = init;
            spin_lock_init(&semaphores[i].lock);
            init_list_head(&semaphores[i].block_queue);
            return i;
        }
    }
    return -1;
}

void do_semaphore_up(int sema_idx)
{
    if (sema_idx < 0 || sema_idx >= SEMAPHORE_NUM || semaphores[sema_idx].key == -1)
        return;

    semaphore_t *sem = &semaphores[sema_idx];
    spin_lock_acquire(&sem->lock);
    sem->val++;
    if (!list_empty(&sem->block_queue))
        do_unblock(sem->block_queue.next);
    spin_lock_release(&sem->lock);
}

void do_semaphore_down(int sema_idx)
{
    if (sema_idx < 0 || sema_idx >= SEMAPHORE_NUM || semaphores[sema_idx].key == -1)
        return;

    semaphore_t *sem = &semaphores[sema_idx];
    while (1) {
        spin_lock_acquire(&sem->lock);
        if (sem->val > 0) {
            sem->val--;
            spin_lock_release(&sem->lock);
            return;
        }
        spin_lock_release(&sem->lock);
        do_block(&current_running->list, &sem->block_queue);
    }
}

void do_semaphore_destroy(int sema_idx)
{
    if (sema_idx < 0 || sema_idx >= SEMAPHORE_NUM || semaphores[sema_idx].key == -1)
        return;

    while (!list_empty(&semaphores[sema_idx].block_queue)) {
        do_unblock(semaphores[sema_idx].block_queue.next);
    }
    semaphores[sema_idx].key = -1;
    semaphores[sema_idx].val = 0;
}

void init_mbox()
{
    for (int i = 0; i < MBOX_NUM; i++) {
        mboxes[i].used = 0;
        mboxes[i].ref = 0;
        mboxes[i].name[0] = '\0';
        spin_lock_init(&mboxes[i].lock);
        mboxes[i].head = 0;
        mboxes[i].tail = 0;
        mboxes[i].size = 0;
        init_list_head(&mboxes[i].send_queue);
        init_list_head(&mboxes[i].recv_queue);
    }
}

int do_mbox_open(char *name)
{
    if (!name)
        return -1;

    for (int i = 0; i < MBOX_NUM; i++) {
        if (mboxes[i].used && strcmp(mboxes[i].name, name) == 0) {
            mboxes[i].ref++;
            return i;
        }
    }

    for (int i = 0; i < MBOX_NUM; i++) {
        if (!mboxes[i].used) {
            mboxes[i].used = 1;
            mboxes[i].ref = 1;
            strncpy(mboxes[i].name, name, sizeof(mboxes[i].name) - 1);
            mboxes[i].name[sizeof(mboxes[i].name) - 1] = '\0';
            mboxes[i].head = 0;
            mboxes[i].tail = 0;
            mboxes[i].size = 0;
            return i;
        }
    }
    return -1;
}

void do_mbox_close(int mbox_idx)
{
    if (mbox_idx < 0 || mbox_idx >= MBOX_NUM || !mboxes[mbox_idx].used)
        return;

    if (mboxes[mbox_idx].ref > 0)
        mboxes[mbox_idx].ref--;

    if (mboxes[mbox_idx].ref == 0) {
        while (!list_empty(&mboxes[mbox_idx].send_queue))
            do_unblock(mboxes[mbox_idx].send_queue.next);
        while (!list_empty(&mboxes[mbox_idx].recv_queue))
            do_unblock(mboxes[mbox_idx].recv_queue.next);
        mboxes[mbox_idx].used = 0;
        mboxes[mbox_idx].name[0] = '\0';
        mboxes[mbox_idx].head = 0;
        mboxes[mbox_idx].tail = 0;
        mboxes[mbox_idx].size = 0;
    }
}

int do_mbox_send(int mbox_idx, void *msg, int msg_length)
{
    if (mbox_idx < 0 || mbox_idx >= MBOX_NUM || !mboxes[mbox_idx].used || msg_length < 0)
        return -1;

    mailbox_t *mb = &mboxes[mbox_idx];
    char *src = (char *)msg;
    int sent = 0;
    int blocked = 0;

    while (sent < msg_length) {
        spin_lock_acquire(&mb->lock);
        while (mb->size == MAX_MBOX_LENGTH) {
            spin_lock_release(&mb->lock);
            blocked++;
            do_block(&current_running->list, &mb->send_queue);
            spin_lock_acquire(&mb->lock);
        }

        int can = MAX_MBOX_LENGTH - mb->size;
        int n = msg_length - sent;
        if (n > can)
            n = can;

        for (int i = 0; i < n; i++) {
            mb->msg[mb->tail] = src[sent + i];
            mb->tail = (mb->tail + 1) % MAX_MBOX_LENGTH;
        }
        mb->size += n;
        sent += n;

        if (!list_empty(&mb->recv_queue))
            do_unblock(mb->recv_queue.next);

        spin_lock_release(&mb->lock);
    }

    return blocked;
}

int do_mbox_recv(int mbox_idx, void *msg, int msg_length)
{
    if (mbox_idx < 0 || mbox_idx >= MBOX_NUM || !mboxes[mbox_idx].used || msg_length < 0)
        return -1;

    mailbox_t *mb = &mboxes[mbox_idx];
    char *dst = (char *)msg;
    int recved = 0;
    int blocked = 0;

    while (recved < msg_length) {
        spin_lock_acquire(&mb->lock);
        while (mb->size == 0) {
            spin_lock_release(&mb->lock);
            blocked++;
            do_block(&current_running->list, &mb->recv_queue);
            spin_lock_acquire(&mb->lock);
        }

        int n = msg_length - recved;
        if (n > mb->size)
            n = mb->size;

        for (int i = 0; i < n; i++) {
            dst[recved + i] = mb->msg[mb->head];
            mb->head = (mb->head + 1) % MAX_MBOX_LENGTH;
        }
        mb->size -= n;
        recved += n;

        if (!list_empty(&mb->send_queue))
            do_unblock(mb->send_queue.next);

        spin_lock_release(&mb->lock);
    }

    return blocked;
}
