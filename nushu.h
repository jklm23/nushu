#ifndef __NUSHU_H_
#define __NUSHU_H_

#define ISN_NEW_BLOCK(seq) (0 | (((tcphdr->dest^tcphdr->source)>>8)&0xff |\
				 ((seq)&0xff) << 8) << 8 | 0xaa << 24)
#define ISN_EMPTY   	(0 | ((tcphdr->source^tcphdr->dest) << 8) | 0xff << 24)

#define ISN_DATA(pktseq,bnum,b0,b1,b2) \
  			((pktseq) << 4 | 0x3 << 2 | ((bnum)&0x3) | \
   			(b0) << 8 | (b1) << 16 | (b2) << 24)

 //#define MAX_PKTS_PER_BLOCK (256 >> 4)
#define MAX_PKTS_PER_BLOCK (4)
#define DATA_BYTES_PER_ISN 3
#define NUSHU_BLOCK_SIZE (MAX_PKTS_PER_BLOCK * DATA_BYTES_PER_ISN)


struct tcphdr;
struct iphdr;

inline int is_ISN_NEW_BLOCK (__u32 isn, struct tcphdr* tcphdr) {
  if (isn&0xff != 0) return 0;
  if (((isn>>8)&0xff) != (((tcphdr->source^tcphdr->dest)>>8)&0xff)) return 0;
  if ((isn>>24)&0xff != 0xaa) return 0;
  return 1;
}

inline int get_new_block_seq (__u32 isn) {
  return ((isn>>16) & 0xff);
}

inline int get_data_bnum (__u32 isn) {
  return (isn&0x3);
}

inline int get_data_seq (__u32 isn) {
  return ((isn>>4)&0x0f); 

}

inline char get_data_byte (__u32 isn, int bno) {
  if (bno < 0 || bno > 2) return 0;
  return ((isn>>(8*(bno+1)))&0xff);
}
	
inline int valid_ISN_DATA (__u32 isn) {
  if ((isn>>2)&0x3 != 0x3) return 0;
  int bnum = get_data_bnum (isn);
  if (bnum == 0) return 0;
  char b1 = get_data_byte (isn, 1);
  if (bnum == 1 && b1 != 0x11) return 0;
  char b2 = get_data_byte (isn, 2);
  if (bnum == 2 && b2 != 0x22) return 0;
  return 1;

}

void nushu_crypt_init(int cipher_mode, char des_key[8]) ;
__u32 nushu_encrypt (__u32 isn, struct iphdr* ip, struct tcphdr *tcp ) ;
__u32 nushu_decrypt (__u32 isn, struct iphdr* ip, struct tcphdr *tcp ) ;



__u32 in_aton(const char *str) ;

#ifdef DEBUG 
#define dbgprintk(x...) printk (x) 
#else
#define dbgprintk(x...) 
#endif

	


#endif


