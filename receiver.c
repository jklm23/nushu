/*
 * Passive Covert Channel using TCPISN field for sending messages.
 * This channel is passive, meaning it does not generate any packets
 * and only changes the packets which are generated by other processes.
 *
 * written by joanna at invisiblethings.org, 2004.
 *
 * credits to kossak and lifeline for the original phrack article
 * of how to use ptype handlers for backdoors implementation
 *
 */

#define MODULE
#define __KERNEL__

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif


#include <linux/kernel.h>
#include <linux/module.h>
#include <net/tcp.h>
#include <net/ip.h>
#include <linux/proc_fs.h>

//#define DEBUG
#include "nushu.h"


struct {
  int bnum;
  char b[3];
  int rcvd;
} packets [MAX_PKTS_PER_BLOCK];
 
int pktseq_in_block = 0;
int last_new_block_seq = 0;

int cipher = 1;	// 0 - no cipher, 1 - des
MODULE_PARM(cipher, "i");
char *key;
MODULE_PARM(key, "s"); 
char des_key[8+1] = "AAAAAAAA";



char *dev, *src_ip;
char *rcv_message = NULL; 
long rcv_msg_buf_sz = 0;
int rcv_ptr = 0;
struct net_device *d;
unsigned long int srcip;
struct packet_type cc_proto;
struct proc_dir_entry *proc_de; 
int first_pkt_received = 0;

MODULE_PARM(dev, "s"); 
MODULE_PARM(src_ip, "s");

__u32 in_aton(const char *);

int cc_read_proc_message (char *buf, char **start, off_t offset, int count,
    int *eof, void *data) {

  int j, i = 0, len = 0;
  *start = NULL;
  rcv_message[rcv_ptr] = 0;
  len += sprintf (buf + len, "%s", rcv_message); 


  for (i = 0; i < MAX_PKTS_PER_BLOCK && packets[i].rcvd; i++) 
	  for (j = 0; j < packets[i].bnum; j++) 
	len += sprintf (buf + len, "%c", packets[i].b[j]);

  
out:
  *eof = 1;
  return len;

}
int cc_read_proc_info (char *buf, char **start, off_t offset, int count,
    int *eof, void *data) {

  int j, i = 0, len = 0;
  *start = NULL;
  len += sprintf (buf + len, "src_ip: %#x\n", srcip); 
  len += sprintf (buf + len, "current packtes[%d]:\n", MAX_PKTS_PER_BLOCK);
  for (i = 0; i < MAX_PKTS_PER_BLOCK; i++) {
	  if (packets[i].rcvd) {
		  len += sprintf (buf + len, "%d: bnum = %d, {", packets[i].bnum);
		  for (j = 0; j < packets[i].bnum; j++) 
			len += sprintf (buf + len, "%c, ", packets[i].b[j]);
		  len += sprintf (buf + len, "\b}\n");
	  } else len += sprintf (buf + len, "%d: not recvd\n", i);
  }
  
out:
  *eof = 1;
  return len;

}



int cc_rcv_func(struct sk_buff *skb, 
    struct net_device *dv,
    struct packet_type *pt) {

  if (ntohs(skb->protocol) != ETH_P_IP) goto out;
  if (skb->nh.iph->protocol != IPPROTO_TCP) goto out;

  struct iphdr *iphdr = skb->nh.iph;
  if (ntohs(iphdr->tot_len) < iphdr->ihl*4 + 3 * 4) goto out; // very fragmeneted packet
  __u32 daddr = iphdr->daddr;
  __u32 saddr = iphdr->saddr;

  if (saddr != srcip) goto out;
  struct tcphdr * tcphdr = (struct tcphdr*)(skb->nh.raw + iphdr->ihl*4);
   
  __u16 sport = tcphdr->source;
  __u16 dport = tcphdr->dest;
  __u16 syn   = tcphdr->syn;
  __u16 fin   = tcphdr->fin;
  __u16 ack   = tcphdr->ack;
  __u16 rst   = tcphdr->rst;

  int i;
  unsigned char c;

  if (syn) {
    __u32 isn = nushu_decrypt (tcphdr->seq, iphdr, tcphdr);

    dbgprintk ("NUSHU: got SYN packet from %#x\n", saddr);

    if (is_ISN_NEW_BLOCK(isn, tcphdr)) {
      dbgprintk ("NUSHU: ISN_NEW_BLOCK\n");
	
      if (!first_pkt_received) {
	      first_pkt_received = 1;
	      goto out;
      }
	
      if (get_new_block_seq(isn) == last_new_block_seq) goto out;
      last_new_block_seq = get_new_block_seq(isn);
      
      if (rcv_ptr + NUSHU_BLOCK_SIZE > rcv_msg_buf_sz) {
	      char *new_rcv_message = kmalloc (rcv_msg_buf_sz + 4096, GFP_KERNEL);
	      if (!new_rcv_message) {
		      printk ("NUSHU: cannot allocate rcv_message buffer!\n");
		      return -1;
	      }

	      memcpy (new_rcv_message, rcv_message, rcv_msg_buf_sz);
	      rcv_msg_buf_sz += 4096;
	      kfree (rcv_message);
	      rcv_message = new_rcv_message;
      }

      for (i = 0; i < MAX_PKTS_PER_BLOCK; i++) {
	if (!packets[i].rcvd) {
	  printk ("nushu: packet no %d in current block not received"
	     	  "but NEW_BLOCK message received!\n", i);
	}
	
	
	if (packets[i].bnum > 0) rcv_message[rcv_ptr++] = packets[i].b[0];
	if (packets[i].bnum > 1) rcv_message[rcv_ptr++] = packets[i].b[1];
	if (packets[i].bnum > 2) rcv_message[rcv_ptr++] = packets[i].b[2];

	packets[i].rcvd = 0;
      }
	  
      pktseq_in_block = 0;

      goto out;
    }
	
    if (!first_pkt_received) goto out;

    if (isn == ISN_EMPTY) {
	 dbgprintk ("NUSHU: ISN_EMPTY\n");
	 goto out; 
    }
  
    
    if (!valid_ISN_DATA(isn)) {
	dbgprintk ("NUSHU: invalid ISN_DATA packet!\n");
	goto out;
    }
    int bnum = get_data_bnum(isn);
    int seq = get_data_seq (isn);
    if (packets[seq].rcvd) goto out;

    packets[seq].rcvd = 1;
    packets[seq].bnum = bnum;
    packets[seq].b[0] = get_data_byte (isn, 0);
    if (bnum > 1) packets[seq].b[1] = get_data_byte (isn, 1);
    if (bnum > 2) packets[seq].b[2] = get_data_byte (isn, 2);


  
  
    
  }
out:
  kfree_skb(skb);
  return 0;
}

int usage () {
  return -ENXIO;
}

int init_module() {
  int msglen;
  if (!src_ip) {
	printk ("src_ip not specified!\n");
	return usage();
  }
  srcip = in_aton(src_ip);

  if (dev) {
    d = dev_get_by_name(dev);
    if (!d) printk("Did not find device %s!\n", dev);
    else cc_proto.dev = d;
  } 

  if (cipher && !key) {
	printk ("cipher key not specified!\n");
	return usage();
  }
  if (cipher && key) strncpy (des_key, key, 8);
  nushu_crypt_init (cipher, des_key);
 

  cc_proto.type = htons(ETH_P_ALL); 
  cc_proto.func = cc_rcv_func;
  dev_add_pack(&cc_proto);

  struct proc_dir_entry *proc_de = proc_mkdir ("nushu", NULL);
  if (!proc_de) {
    printk ("Error creating proc entry!\n");
    return -1;
  }


  create_proc_read_entry ("message_received", 0, proc_de, cc_read_proc_message, NULL);
  create_proc_read_entry ("info", 0, proc_de, cc_read_proc_info, NULL);

  rcv_message = kmalloc (4096, GFP_KERNEL);
  if (!rcv_message) {
    printk ("NUSHU: cannot allocate rcv_message buffer!\n");
    return -1;
  }
	
  rcv_msg_buf_sz = 4096;

  return(0);
}

void cleanup_module() {
  dev_remove_pack(&cc_proto);
  remove_proc_entry ("info", proc_de);
  remove_proc_entry ("message_received", proc_de);
  remove_proc_entry ("nushu", NULL);
  if (rcv_message) kfree (rcv_message);
  
  dbgprintk("NUSHU unloaded\n");

}

MODULE_LICENSE("GPL");


