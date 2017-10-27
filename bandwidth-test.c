#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mmio.h"
#include "nic.h"

#define PACKET_WINDOW 10
#define PACKET_WORDS 180

uint64_t out_packets[PACKET_WINDOW][PACKET_WORDS];
uint64_t in_packets[PACKET_WINDOW][3];
char inflight[PACKET_WINDOW];
uint16_t checksums[PACKET_WINDOW];

static unsigned long rdcycle(void)
{
	unsigned long cycles;

	asm volatile ("rdcycle %[cycles]" : [cycles] "=r" (cycles));
	return cycles;
}

static void fill_packet(
	uint64_t *packet, uint64_t srcmac, uint64_t dstmac, int id)
{
	packet[0] = dstmac << 16;
	packet[1] = srcmac | (0x1008L << 48);
	packet[2] = id << 16;

	for (int i = 3; i < PACKET_WORDS; i++)
		packet[i] = random();
}

static inline void post_send(uint64_t addr, uint64_t len)
{
	uint64_t request = ((len & 0x7fff) << 48) | (addr & 0xffffffffffffL);
	reg_write64(SIMPLENIC_SEND_REQ, request);
}

void complete_sends(void)
{
	uint64_t out_sends = PACKET_WINDOW;

	while (out_sends > 0) {
		int comp_avail = nic_send_comp_avail();
		for (int i = 0; i < comp_avail; i++) {
			reg_read16(SIMPLENIC_SEND_COMP);
			inflight[i] = 1;
			out_sends--;
		}
	}
}

void complete_recvs(void)
{
	uint64_t out_recvs = PACKET_WINDOW;
	uint64_t payload;
	int recv_id = 0, send_id;

	while (out_recvs > 0) {
		int comp_avail = nic_recv_comp_avail();
		for (int i = 0; i < comp_avail; i++) {
			reg_read16(SIMPLENIC_RECV_COMP);
			payload = in_packets[recv_id][2];
			send_id = payload >> 16;
			inflight[send_id] = 0;
			checksums[send_id] = (payload & 0xffff);
			recv_id++;
			out_recvs--;
		}
	}
}

int verify_checksum(uint16_t *data, int n, uint16_t cksum)
{
	uint32_t sum = cksum;

	for (int i = 0; i < n; i++)
		sum += data[i];

	while ((sum >> 16) != 0)
		sum = (sum & 0xffff) + (sum >> 16);

	return ~sum & 0xffff;
}

int main(void)
{
	uint64_t srcmac = nic_macaddr();
	uint64_t dstmac = srcmac + (1L << 40);
	uint64_t start = 0, end = 0;
	uint16_t cksum;

	srandom(0xCFF32987);

	for (int i = 0; i < PACKET_WINDOW; i++) {
		fill_packet(out_packets[i], srcmac, dstmac, i);
		memset(in_packets[i], 0, sizeof(uint64_t) * 3);
	}

	asm volatile ("fence");

	printf("Start bandwidth test\n");

	start = rdcycle();

	for (int i = 0; i < PACKET_WINDOW; i++) {
		post_send(
			(uint64_t) out_packets[i],
			PACKET_WORDS * sizeof(uint64_t));
		reg_write64(SIMPLENIC_RECV_REQ, (uint64_t) in_packets[i]);
	}

	complete_sends();
	complete_recvs();

	end = rdcycle();

	for (int i = 0; i < PACKET_WINDOW; i++) {
		int cksum;
		if (inflight[i])
			printf("Packet %d was never acked\n", i);
		cksum = verify_checksum(
				(uint16_t *) out_packets[i],
				PACKET_WORDS * 4,
				checksums[i]);
		if (cksum)
			printf("Packet %d checksum %x was wrong\n", i, checksums[i]);
	}

	printf("Send/Recv took %lu cycles\n", end - start);

	return 0;
}
