
#define __KERNEL__

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif


#include <linux/kernel.h>

/*
 *      Convert an ASCII string to binary IP.
 */

__u32 in_aton(const char *str) {
  unsigned long l;
  unsigned int val;
  int i;

  l = 0;
  for (i = 0; i < 4; i++) {
    l <<= 8;
    if (*str != '\0') {
      val = 0;
      while (*str != '\0' && *str != '.') {
	val *= 10;
	val += *str - '0';
	str++;
      }
      l |= val;
      if (*str != '\0')
	str++;
    }
  }
  return(htonl(l));
}


