/*
** lkh.h
** 
** Made by <mayhem@epita.fr>
** 
** Last update Sat Dec  8 10:58:54 2001 mayhem
*/

#define	__KERNEL__
#define	MODULE
#define	LINUX

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/unistd.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>


extern void *sys_call_table[];
extern void *kmalloc(size_t size, int mode);
//extern void kfree(void *);

/*
** some needed opcodes
*/

#define	NOP			0x90
#define	CALL			0xE8
#define	MOV_EAX			0xB8
#define	IJMP1			0xFF
#define	IJMP2			0xE0
#define	LEAVE			0xC9

/*
** basic shorters
*/

#define	QUIT(str)		{ printk(KERN_ALERT "%s", str); return (NULL); }


/*
** Runtime fixing constants
*/

#define	REL_OFFSET_ONE		1
#define	REL_OFFSET_TWO		6
#define	REL_OFFSET_THREE	84
#define	REL_OFFSET_FOUR		96
#define	REL_OFFSET_FIVE		101

/*
** Used for the aggressive mode
*/
#define	CACHE1_OFFSET		REL_OFFSET_THREE - 17
#define	CACHE1_SIZE		28
#define	CACHE2_OFFSET		REL_OFFSET_FIVE + 13
#define	CACHE2_SIZE		6

/*
** Used for hooking on non stackframed functions .
*/
#define	FIXUP_ONE		22
#define	FIXUP_TWO		74
#define	FIXUP_THREE		81


/*
** General information
*/
#define	INTRO_CODE_SIZE		24
#define	OUTRO_CODE_OFFSET	HOOK_CONSTANT1_OFFSET + HOOK_CONSTANT1_SIZE
#define	OUTRO_CODE_SIZE		8
#define	HOOK_SIZE		OUTRO_CODE_OFFSET + OUTRO_CODE_SIZE



/*
** Hook types, states and modes
*/
#define	HOOK_PERMANENT		1
#define	HOOK_SINGLESHOT		2
#define	HOOK_ENABLED		4
#define	HOOK_DISABLED		8
#define	HOOK_DISCRETE		16
#define	HOOK_AGGRESSIVE		32



/*
** 804876f:       1e                      push   %ds
** 8048770:       07                      pop    %es
** 8048771:       fc                      cld    
** 8048772:       31 c9                   xor    %ecx,%ecx
** 8048774:       b1 07                   mov    $0x7,%cl
** 8048776:       f3 a4                   repz movsb %ds:(%esi),%es:(%edi)
**
** The constant is used in khook_set_type() to change the hook's 
** attributes concerning the residency (permanent or singleshot) 
**
*/ 
#define	HOOK_CONSTANT1			"\x1E\x07\xFC\x31\xC9\xB1\x07\xF3\xA4"
#define	HOOK_CONSTANT1_SIZE		9
#define	HOOK_CONSTANT1_OFFSET		REL_OFFSET_FIVE + 4





/*
** 8048b90:       55                      push   %ebp
** 8048b91:       89 e5                   mov    %esp,%ebp
**
** Used in  khook_set_state() to know if the hijacked code has been
** compiled with or without frame pointer .
*/
#define	STACKFRAME_CONSTANT		"\x55\x89\xE5"
#define	STACKFRAME_CONSTANT_SIZE	3








/*
** Structures
*/
typedef struct	s_hook
{
  int		addr;			/* address of the hooked function      */
  int		offset;			/* size of the stackframe (can be 0)   */
  char		saved_bytes[7];		/* first addr's 7 bytes before hooking */
  char	        voodoo_bytes[7];	/* first addr's 7 bytes after hooking  */
  char		hook[HOOK_SIZE];	/* hook code			       */

  char		cache1[CACHE1_SIZE];    /* used for aggressive mode	       */
  char		cache2[CACHE2_SIZE];	/* used for aggressive mode	       */	

}		hook_t;





/*
** Prototypes
*/
hook_t	*khook_create(int fct, int mask);
void	khook_destroy(hook_t *h);
void	khook_purge(hook_t *h);
int	khook_add_entry(hook_t *h, int routine, int range);
int	khook_remove_entry(hook_t *h, int range);
int	khook_set_type(hook_t *h, char type);
int	khook_set_state(hook_t *h, char state);
int	khook_set_mode(hook_t *h, char mode);
int	khook_set_attr(hook_t *h, int mask);
int	ksym_lookup(char *name);














