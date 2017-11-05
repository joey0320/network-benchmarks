#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mmio.h"
#include "nic.h"
#include "util.h"

#define NPACKETS 100
#define PACKET_WORDS 180

uint64_t out_packets[NPACKETS][PACKET_WORDS];
uint64_t in_packet[3];
char completed[NPACKETS];
int total_comp = 0;
int total_req = 0;

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
	uint16_t counts, send_req, send_comp;
	static int req_id = 0;
	static int comp_id = 0;

	counts = reg_read16(SIMPLENIC_COUNTS);
	send_req  = counts & 0xf;
	send_comp = (counts >> 8)  & 0xf;

	for (int i = 0; i < send_req && total_req < NPACKETS; i++) {
		post_send((uint64_t) out_packets[req_id], PACKET_WORDS * 8);
		req_id++;
		total_req++;
	}

	for (int i = 0; i < send_comp && total_comp < NPACKETS; i++) {
		reg_read16(SIMPLENIC_SEND_COMP);
		completed[comp_id] = 1;
		comp_id++;
		total_comp++;
	}
}

int main(void)
{
	uint64_t srcmac = nic_macaddr();
	uint64_t dstmac = SERVER_MACADDR;
	uint64_t start = 0, end = 0;

	srandom(0xCFF32987);

        memset(in_packet, 0, sizeof(uint64_t) * 3);
	memset(completed, 0, NPACKETS);
	for (int i = 0; i < NPACKETS; i++) {
		fill_packet(out_packets[i], srcmac, dstmac, i);
	}

	asm volatile ("fence");

	printf("Start bandwidth test\n");

	start = rdcycle();

        reg_write64(SIMPLENIC_RECV_REQ, (uint64_t) in_packet);

        while (total_comp < NPACKETS)
            process_loop();

	nic_recv(in_packet);

	end = rdcycle();

	for (int i = 0; i < NPACKETS; i++) {
		if (!completed[i])
			printf("Packet %d was not sent\n", i);
	}

	printf("Send/Recv took %lu cycles\n", end - start);

	nic_recv(in_packet);

	return 0;
}
