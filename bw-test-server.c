#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "nic.h"
#include "util.h"

#define PACKET_WORDS 180

uint64_t in_packet[PACKET_WORDS];
uint64_t out_packet[3];

int main(void)
{
	int len, id;
	uint16_t cksum;
	uint64_t dstmac, srcmac;
	uint16_t ethtype;

	for (;;) {
		len = nic_recv(in_packet);
		dstmac = in_packet[0] >> 16;
		srcmac = in_packet[1] & ((1 << 48) - 1);
		ethtype = in_packet[1] >> 48;
		id = in_packet[2] >> 16;
		cksum = compute_checksum(in_packet, len/2, 0);

		out_packet[0] = srcmac << 16;
		out_packet[1] = dstmac | (((uint64_t) ethtype) << 48);
		out_packet[2] = (id << 16) | cksum;
		nic_send(out_packet, sizeof(out_packet));
	}
}
