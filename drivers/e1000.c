#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <os/mm.h>
#include <assert.h>
#include <pgtable.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors — dynamically allocated from DMA-safe region
static struct e1000_tx_desc *tx_desc_array;
static struct e1000_rx_desc *rx_desc_array;

// E1000 Tx & Rx packet buffer — dynamically allocated from DMA-safe region
static char (*tx_pkt_buffer)[TX_PKT_SIZE];
static char (*rx_pkt_buffer)[RX_PKT_SIZE];

/* DMA-safe allocator: returns KVA from user-physical region (0x52100000+),
   which is real RAM accessible by E1000 DMA, unlike kernel BSS (0x502xxxxx
   boot ROM) where DMA writes are silently dropped. */
static ptr_t dma_cur = 0xffffffc052100000;  // phys 0x52100000

static void *dma_alloc(int num_pages) {
    ptr_t ret = dma_cur;
    dma_cur += num_pages * PAGE_SIZE;
    memset((void *)ret, 0, num_pages * PAGE_SIZE);
    return (void *)ret;
}

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x10, 0x53, 0x00, 0x30, 0x83};

// Current head/tail indices for tx and rx rings
static int tx_tail = 0;
static int rx_tail = 0;

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
    /* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

    /* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

    /* Delay for pending PCI transactions */
    latency(1);

    /* Clear interrupt mask */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;

    /* Reset tx/rx tail indices */
    tx_tail = 0;
    rx_tail = 0;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    int i;

    /* Initialize tx descriptors: point each descriptor's buffer address
     * to the corresponding tx_pkt_buffer slot. Use physical address. */
    for (i = 0; i < TXDESCS; i++) {
        tx_desc_array[i].addr   = kva2pa((uintptr_t)tx_pkt_buffer[i]);
        tx_desc_array[i].length = 0;
        tx_desc_array[i].cso    = 0;
        tx_desc_array[i].cmd    = 0;
        tx_desc_array[i].status = E1000_TXD_STAT_DD;  /* DD=1: available for driver */
        tx_desc_array[i].css    = 0;
        tx_desc_array[i].special = 0;
    }

    /* Set up the Tx descriptor base address (physical) and length */
    e1000_write_reg(e1000, E1000_TDBAL, (uint32_t)kva2pa((uintptr_t)tx_desc_array));
    e1000_write_reg(e1000, E1000_TDBAH, (uint32_t)(kva2pa((uintptr_t)tx_desc_array) >> 32));
    e1000_write_reg(e1000, E1000_TDLEN, TXDESCS * sizeof(struct e1000_tx_desc));

    /* Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* Program the Transmit Control Register: enable + pad short packets.
     * CT = 0x10 (collision threshold), COLD = 0x40 (collision distance) */
    e1000_write_reg(e1000, E1000_TCTL,
        E1000_TCTL_EN | E1000_TCTL_PSP |
        (0x10 << 4) | (0x40 << 12));

    /* Set inter-packet gap */
    e1000_write_reg(e1000, E1000_TIPG,
        (DEFAULT_82543_TIPG_IPGT_COPPER) |
        (DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT) |
        (DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT));
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    int i;
    uint32_t ral, rah;

    /* Set e1000 MAC Address to RAR[0] (Receive Address Register 0) */
    /* RAL[0]: lower 32 bits of MAC address */
    ral = enetaddr[0] | (enetaddr[1] << 8) | (enetaddr[2] << 16) | (enetaddr[3] << 24);
    e1000_write_reg_array(e1000, E1000_RA, 0, ral);

    /* RAH[0]: upper 16 bits of MAC + AV (Address Valid) bit */
    rah = enetaddr[4] | (enetaddr[5] << 8) | E1000_RAH_AV;
    e1000_write_reg_array(e1000, E1000_RA, 1, rah);

    /* Initialize rx descriptors: point to rx_pkt_buffer slots */
    for (i = 0; i < RXDESCS; i++) {
        rx_desc_array[i].addr   = kva2pa((uintptr_t)rx_pkt_buffer[i]);
        rx_desc_array[i].length = 0;
        rx_desc_array[i].csum   = 0;
        rx_desc_array[i].status = 0;
        rx_desc_array[i].errors = 0;
        rx_desc_array[i].special = 0;
    }

    /* Set up the Rx descriptor base address (physical) and length */
    e1000_write_reg(e1000, E1000_RDBAL, (uint32_t)kva2pa((uintptr_t)rx_desc_array));
    e1000_write_reg(e1000, E1000_RDBAH, (uint32_t)(kva2pa((uintptr_t)rx_desc_array) >> 32));
    e1000_write_reg(e1000, E1000_RDLEN, RXDESCS * sizeof(struct e1000_rx_desc));

    /* Set up the HW Rx Head and Tail descriptor pointers.
     * For receive, we own all descriptors initially (tail = count-1). */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);
    rx_tail = RXDESCS - 1;

    /* Program the Receive Control Register:
     * Enable, Broadcast Accept Mode, Unicast Promiscuous,
     * 2048-byte buffer size.  UPE is needed because pktRxTx
     * sends to a hard-coded destination MAC that differs from
     * our E1000 MAC. */
    e1000_write_reg(e1000, E1000_RCTL,
        E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_UPE |
        E1000_RCTL_SZ_2048);
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Probe: read STATUS to check if device is present.
     * Without -device e1000, this returns 0xFFFFFFFF or 0. */
    uint32_t probe = e1000_read_reg(e1000, E1000_STATUS);
    if (probe == 0xFFFFFFFF || probe == 0) {
        return;  /* device not present */
    }

    /* Allocate descriptor rings and packet buffers from DMA-safe region */
    tx_desc_array  = dma_alloc((TXDESCS * sizeof(struct e1000_tx_desc)) / PAGE_SIZE + 1);
    rx_desc_array  = dma_alloc((RXDESCS * sizeof(struct e1000_rx_desc)) / PAGE_SIZE + 1);
    tx_pkt_buffer  = dma_alloc((TXDESCS * TX_PKT_SIZE) / PAGE_SIZE + 1);
    rx_pkt_buffer  = dma_alloc((RXDESCS * RX_PKT_SIZE) / PAGE_SIZE + 1);
    printk("> [E1000] DMA buffers at va=0x%lx pa=0x%lx\n",
           (uintptr_t)tx_desc_array, kva2pa((uintptr_t)tx_desc_array));

    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();

    printk("> [E1000] STATUS=0x%x RCTL=0x%x RAL0=0x%x\n",
           e1000_read_reg(e1000, E1000_STATUS),
           e1000_read_reg(e1000, E1000_RCTL),
           e1000_read_reg_array(e1000, E1000_RA, 0));

    /* RX interrupts kept masked for now — polling mode */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);


}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    int tail;
    uint32_t tctl;

    if (length <= 0 || length > TX_PKT_SIZE)
        return 0;

    /* Read current tail index from hardware */
    tail = e1000_read_reg(e1000, E1000_TDT);
    int old_tail = tail;

    /* Check if the descriptor is done (available) */
    if (!(tx_desc_array[tail].status & E1000_TXD_STAT_DD)) {
        /* If not done, wait for it to complete */
        uint64_t timeout = get_ticks() + 100000;
        while (!(tx_desc_array[tail].status & E1000_TXD_STAT_DD)) {
            if (get_ticks() > timeout) return 0;
        }
    }

    /* Copy packet data to the tx buffer for this descriptor */
    memcpy(tx_pkt_buffer[tail], txpacket, length);

    /* Set up transmit descriptor */
    tx_desc_array[tail].length = (uint16_t)length;
    tx_desc_array[tail].cso    = 0;
    tx_desc_array[tail].cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    tx_desc_array[tail].status = 0;
    tx_desc_array[tail].css    = 0;

    /* Advance tail and update hardware */
    tail = (tail + 1) % TXDESCS;
    e1000_write_reg(e1000, E1000_TDT, tail);

    /* Make sure TX is enabled */
    tctl = e1000_read_reg(e1000, E1000_TCTL);
    if (!(tctl & E1000_TCTL_EN))
        e1000_write_reg(e1000, E1000_TCTL, tctl | E1000_TCTL_EN);

    /* Wait for TX completion with debug */
    {
        uint64_t t0 = get_ticks();
        while (!(tx_desc_array[old_tail].status & E1000_TXD_STAT_DD)) {
            if (get_ticks() - t0 > 500000) break;
        }
        printk("> [TX] len=%d DD=%d TDH=%d TDT=%d\n",
               length, !!(tx_desc_array[old_tail].status & E1000_TXD_STAT_DD),
               e1000_read_reg(e1000, E1000_TDH),
               e1000_read_reg(e1000, E1000_TDT));
    }

    /* Wait for the descriptor to be done */
    {
        uint64_t timeout = get_ticks() + 100000;
        int prev = (tail == 0) ? (TXDESCS - 1) : (tail - 1);
        while (!(tx_desc_array[prev].status & E1000_TXD_STAT_DD)) {
            if (get_ticks() > timeout) return 0;
        }
    }

    return length;
}

/**
 * e1000_poll - Receive packet through e1000 net device (polling mode)
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet, 0 if no packet available
 **/
int e1000_poll(void *rxbuffer)
{
    int tail, next;

    /* Compute next descriptor index (advance tail by 1, wrapping) */
    tail = rx_tail;
    next = (tail + 1) % RXDESCS;

    /* Check if the next descriptor has been filled by hardware (DD bit) */
    if (!(rx_desc_array[next].status & E1000_RXD_STAT_DD))
        return 0;  /* No packet available */

    /* Get packet length and copy data */
    uint16_t length = rx_desc_array[next].length;
    if (length > 0 && length <= RX_PKT_SIZE) {
        memcpy(rxbuffer, rx_pkt_buffer[next], length);
    }

    /* Mark descriptor as available again */
    rx_desc_array[next].status = 0;

    /* Update tail pointer: give descriptor back to hardware */
    rx_tail = next;
    e1000_write_reg(e1000, E1000_RDT, next);

    return (int)length;
}
