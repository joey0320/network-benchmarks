#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "nic.h"
#include "util.h"

#define PACKET_WORDS 180
#define PACKET_WINDOW 10
#define NREPEATS 10
#define TOTAL_PACKETS (NREPEATS * PACKET_WINDOW)

uint64_t in_packets[PACKET_WINDOW][PACKET_WORDS];
uint64_t out_packet[3];
int recv_counts[PACKET_WINDOW];
int send_counts[PACKET_WINDOW];
int total_recv = 0;

static inline void post_send(uint64_t addr, uint64_t len)
{
	uint64_t request = ((len & 0x7fff) << 48) | (addr & 0xffffffffffffL);
	reg_write64(SIMPLENIC_SEND_REQ, request);
}

static inline void process_recv(int recv_id)
{
        int len, send_id;

        len = reg_read16(SIMPLENIC_RECV_COMP);
        if (len < 24) {
                printf("Packet too small\n");
                abort();
        }
        recv_counts[recv_id]++;
        total_recv++;
        send_id = in_packets[recv_id][2];
        send_counts[send_id]++;

        if (recv_counts[recv_id] < NREPEATS)
                reg_write64(SIMPLENIC_RECV_REQ, (uint64_t) in_packets[recv_id]);
}

static inline void process_loop(void)
{
        uint16_t recv_comp;
        static int recv_id = 0;

        recv_comp = nic_recv_comp_avail();

        for (int i = 0; i < recv_comp; i++) {
                process_recv(recv_id);
                recv_id = (recv_id + 1) % PACKET_WINDOW;
        }
}

int main(void)
{
        uint64_t srcmac, dstmac;

        srcmac = nic_macaddr();
        dstmac = srcmac - (1L << 40);

        out_packet[0] = dstmac << 16;
        out_packet[1] = srcmac | (0x1008L << 48);
        out_packet[2] = 0;

        for (int i = 0; i < PACKET_WINDOW; i++) {
                recv_counts[i] = 0;
                send_counts[i] = 0;
                reg_write64(SIMPLENIC_RECV_REQ, (uint64_t) in_packets[i]);
        }

        while (total_recv < TOTAL_PACKETS)
                process_loop();

        nic_send(out_packet, 24);

        for (int i = 0; i < PACKET_WINDOW; i++) {
                if (recv_counts[i] < NREPEATS)
                        printf("Only received to %d %d times\n",
                                        i, recv_counts[i]);
                if (send_counts[i] < NREPEATS)
                        printf("Only received send id %d %d times\n",
                                        i, send_counts[i]);
        }

        return 0;
}
