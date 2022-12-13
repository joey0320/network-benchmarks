#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mmio.h"
#include "nic.h"
#include "encoding.h"

// An ICMP packet is 44 bytes
#define PACKET_BYTES 44
#define PACKET_WORDS 8

uint64_t out_packet[PACKET_WORDS];
uint64_t in_packet[PACKET_WORDS];

int main(void)
{
  printf("latency-test started\n");

	uint64_t srcmac = nic_macaddr(), start, end;

  printf("got macaddr\n");

	out_packet[0] = srcmac << 16;
	out_packet[1] = srcmac | (0x0208L << 48);

	nic_post_recv((uint64_t) in_packet);
  printf("nic_post_recv done\n");

	start = rdcycle();
	nic_post_send((uint64_t) out_packet, PACKET_BYTES);
  printf("nic_post_send done\n");

  printf("nic_counts: %d\n", nic_counts());

	while (((nic_counts() >> NIC_COUNT_RECV_COMP) & 0xf) == 0);
	nic_complete_recv();
  printf("nic_complete_recv done\n");

	end = rdcycle();

	while (((nic_counts() >> NIC_COUNT_SEND_COMP) & 0xf) == 0);
	nic_complete_send();
  printf("nic_complete_send done\n");

	printf("Total send/recv time %lu\n", end - start);

	return 0;
}
