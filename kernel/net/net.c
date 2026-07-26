#include <e1000.h>
#include <type.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/irq.h>
#include <printk.h>

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

int do_net_send(void *txpacket, int length)
{
    return e1000_transmit(txpacket, length);
}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    int total = 0;
    /* Zero out lengths so the caller sees clean data on short reads */
    if (pkt_lens) {
        for (int i = 0; i < pkt_num; i++) pkt_lens[i] = 0;
    }
    for (int i = 0; i < pkt_num; i++) {
        int len = e1000_poll(rxbuffer + total);
        if (len <= 0) break;
        if (pkt_lens) pkt_lens[i] = len;
        total += len;
    }
    return total;
}

void net_handle_irq(void)
{
    /* Read the interrupt cause register to determine what fired */
    uint32_t icr = e1000_read_reg(e1000, E1000_ICR);

    /* RX interrupt: wake up tasks waiting to receive */
    if (icr & (E1000_ICR_RXT0 | E1000_ICR_RXDMT0)) {
        while (!list_empty(&recv_block_queue)) {
            list_node_t *node = recv_block_queue.next;
            do_unblock(node);
        }
    }

    /* TX interrupt: wake up tasks waiting to send */
    if (icr & E1000_ICR_TXDW) {
        while (!list_empty(&send_block_queue)) {
            list_node_t *node = send_block_queue.next;
            do_unblock(node);
        }
    }

    /* Link status change (cable plugged/unplugged) */
    if (icr & E1000_ICR_LSC) {
        printk("> [NET] Link status changed.\n");
    }
}
