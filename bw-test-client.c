#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mmio.h"
#include "nic.h"
#include "util.h"

#define PACKET_WINDOW 10
#define PACKET_WORDS 180
#define NREPEATS 10

uint64_t out_packets[PACKET_WINDOW][PACKET_WORDS];
uint64_t in_packet[3];

int sent_counts[PACKET_WINDOW];
int total_sent = 0;

static void fill_packet(
	uint64_t *packet, uint64_t srcmac, uint64_t dstmac, int id)
{
	packet[0] = dstmac << 16;
	packet[1] = srcmac | (0x1008L << 48);
	packet[2] = id;

	for (int i = 3; i < PACKET_WORDS; i++)
		packet[i] = random();
}

static inline void post_send(uint64_t addr, uint64_t len)
{
	uint64_t request = ((len & 0x7fff) << 48) | (addr & 0xffffffffffffL);
	reg_write64(SIMPLENIC_SEND_REQ, request);
}

static void process_loop(void)
{
    uint16_t send_comp;
    static int send_id = 0;

    send_comp = nic_send_comp_avail();

    for (int i = 0; i < send_comp; i++) {
        reg_read16(SIMPLENIC_SEND_COMP);
        sent_counts[send_id]++;
        total_sent++;

        if (sent_counts[send_id] < NREPEATS)
                post_send((uint64_t) out_packets[send_id], PACKET_WORDS * 8);

        send_id = (send_id + 1) % PACKET_WINDOW;
    }
}

int main(void)
{
	uint64_t srcmac = nic_macaddr();
	uint64_t dstmac = srcmac + (1L << 40);
	uint64_t start = 0, end = 0;

	srandom(0xCFF32987);

        memset(in_packet, 0, sizeof(uint64_t) * 3);
	for (int i = 0; i < PACKET_WINDOW; i++) {
		fill_packet(out_packets[i], srcmac, dstmac, i);
                sent_counts[i] = 0;
	}

	asm volatile ("fence");

	printf("Start bandwidth test\n");

	start = rdcycle();

        reg_write64(SIMPLENIC_RECV_REQ, (uint64_t) in_packet);

	for (int i = 0; i < PACKET_WINDOW; i++) {
		post_send(
			(uint64_t) out_packets[i],
			PACKET_WORDS * sizeof(uint64_t));
	}

        while (total_sent < NREPEATS * PACKET_WINDOW)
            process_loop();

        while (nic_recv_comp_avail() == 0) {}
        reg_read16(SIMPLENIC_RECV_COMP);

	end = rdcycle();

	for (int i = 0; i < PACKET_WINDOW; i++) {
		if (sent_counts[i] < NREPEATS)
			printf("Packet %d was only acked %d times\n",
                                i, sent_counts[i]);
	}

	printf("Send/Recv took %lu cycles\n", end - start);

	return 0;
}
