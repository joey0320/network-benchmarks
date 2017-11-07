#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "nic.h"
#include "util.h"

uint64_t in_packets[NPACKETS][PACKET_WORDS];
uint64_t out_packet[3];
uint64_t expected_packets[NPACKETS][PACKET_WORDS];
char completed[NPACKETS];
int recvid2sendid[NPACKETS];
int total_req = 0;
int total_comp = 0;

static inline void fill_packets(uint64_t *packets)
{
	for (int i = 3; i < PACKET_WORDS; i++)
		packets[i] = random();
}

static inline void check_data(int recv_id)
{
	int send_id = recvid2sendid[recv_id];

	for (int i = 3; i < PACKET_WORDS; i++) {
		if (in_packets[recv_id][i] != expected_packets[send_id][i]) {
			printf("Data mismatch in packet %d\n", send_id);
			abort();
		}
	}
}

static inline void post_send(uint64_t addr, uint64_t len)
{
	uint64_t request = ((len & 0x7fff) << 48) | (addr & 0xffffffffffffL);
	reg_write64(SIMPLENIC_SEND_REQ, request);
}

static inline void process_loop(void)
{
        uint16_t counts, recv_req, recv_comp;
        static int req_id = 0, comp_id = 0, send_id;
	static int time_count = 0;
	int len;

	counts = reg_read16(SIMPLENIC_COUNTS);
	recv_req  = (counts >> 4) & 0xf;
        recv_comp = (counts >> 12) & 0xf;

	for (int i = 0; i < recv_req && total_req < NPACKETS; i++) {
		reg_write64(SIMPLENIC_RECV_REQ, (uint64_t) in_packets[req_id]);
		req_id++;
		total_req++;
	}

        for (int i = 0; i < recv_comp && total_comp < NPACKETS; i++) {
		len = reg_read16(SIMPLENIC_RECV_COMP);
		if (len < 24) {
			printf("Packet too smal\n");
			abort();
		}
		send_id = in_packets[comp_id][2];
		completed[send_id] = 1;
		recvid2sendid[comp_id] = send_id;
		comp_id++;
		total_comp++;
        }

	time_count++;
	if (time_count == 100) {
		printf("Processing\n");
	}
}

int main(void)
{
        uint64_t srcmac, dstmac;

        srcmac = nic_macaddr();
        dstmac = CLIENT_MACADDR;

        out_packet[0] = dstmac << 16;
        out_packet[1] = srcmac | (0x1008L << 48);
        out_packet[2] = 0;

	memset(completed, 0, NPACKETS);

	srandom(0xCFF32987);

	for (int i = 0; i < NPACKETS; i++)
		fill_packets(expected_packets[i]);

        while (total_comp < NPACKETS)
                process_loop();

        nic_send(out_packet, 24);

	printf("Finished sending data\n");

        for (int i = 0; i < NPACKETS; i++) {
		if (!completed[i])
			printf("Did not receive packet %d\n", i);
		check_data(i);
        }

	printf("All data correct.\n");

	nic_send(out_packet, 24);

        return 0;
}
