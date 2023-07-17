#define MODULE
#define __KERNEL__

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif


#include <linux/kernel.h>
#include <net/tcp.h>
#include <net/ip.h>

#include "d3des.h"

int cipher;	// 0 - no cipher, 1 - des

void nushu_crypt_init(int cipher_mode, char des_key[8]) {
  cipher = cipher_mode;
  if (!cipher) return;
  deskey (des_key, EN0); 
}

__u32 nushu_encrypt (__u32 isn, struct iphdr* ip, struct tcphdr *tcp ) {
  if (!cipher) return isn;
  char seed[8];
  strcpy (&seed[0], "NU");
  *((__u16*)&seed[2]) = tcp->source ^ tcp->dest;
  *((__u32*)&seed[4]) = ip->saddr ^ ip->daddr;

  char hash[8];
  des (seed, hash);

  __u32 key = (*(__u32*)hash);
 
  return key ^ isn; 
}


__u32 nushu_decrypt (__u32 isn, struct iphdr* ip, struct tcphdr *tcp ) {
  if (!cipher) return isn;
  char seed[8];
  strcpy (&seed[0], "NU");
  *((__u16*)&seed[2]) = tcp->source ^ tcp->dest;
  *((__u32*)&seed[4]) = ip->saddr ^ ip->daddr;

  char hash[8];
  des (seed, hash);

  __u32 key = (*(__u32*)hash);
  return key ^ isn;

}


