/*
**
** LKH !%$@# 
**
** Just #include "lkh.c" in your lkm .
**
** Made by <mayhem@hert.org>
**
** Last update Fri Jan 18 17:44:59 2002 mayhem
**
*/
#include "lkh.h"





/* 
** Function only used with objdump during my userland development 
**
*/
void		asmcode()
{

      
    __asm__("movl	$0x00, %esi		\n\t"
	    "movl	$0x00, %edi		\n\t"
	    "push	%ds			\n\t"
	    "pop	%es			\n\t"
	    "cld				\n\t"
	    "xor	%ecx, %ecx		\n\t"
	    "movb	$0x07, %cl		\n\t"
	    "rep	movsb			\n\t"

	    "leal	8(%esp), %eax		\n\t"
	    "push	%eax			\n\t"
	    "nop; nop; nop; nop; nop		\n\t"
	    "nop; nop; nop; nop; nop		\n\t"
	    "nop; nop; nop; nop; nop		\n\t"
	    "nop; nop; nop; nop; nop		\n\t"
	    "nop; nop; nop; nop; nop		\n\t"
	    "nop; nop; nop; nop; nop		\n\t"
	    "nop; nop; nop; nop; nop		\n\t"
	    "nop; nop; nop; nop; nop		\n\t"
	    "add	$0x04, %esp		\n\t"

	    "dec	%esp			\n\t"
	    "jmp	end			\n\t"
	    "begin:				\n\t" 
	    "pop	%esi			\n\t"
	    "movl	5(%esp), %eax		\n\t"
	    "movl	%eax, 20(%esi)		\n\t"
	    "movl	%esi, 5(%esp)		\n\t"
	    "inc	%esp			\n\t"
	    "movl	$0x00, %eax		\n\t"
	    "jmp	*%eax			\n\t"
	    "end:				\n\t"
	    "call	begin			\n\t"

	    "movl	$0x00, %esi		\n\t"
	    "movl	$0x00, %edi		\n\t"
	    "push	%ds			\n\t"
	    "pop	%es			\n\t"
	    "cld				\n\t"
	    "xor	%ecx, %ecx		\n\t"
	    "movb	$0x07, %cl		\n\t"
	    "rep	movsb			\n\t"

	    "movl	$0x00, %ecx	        \n\t"
	    "pushl	%ecx			\n\t"
	    "ret				\n\t");
	    
}
    

/*
******************************************
******************************************
****************** END *******************
******************************************
******************************************
*/















/*
** ADD A CALLBACK FOR THAT HOOK
** -> hook_functions_list[range] = new_routine;
*/
int	khook_add_entry(hook_t *h, int new_routine, int range)
{
  int	off;

  if (range < 0 && range > 7)
    return (-1);
  
  h->hook[INTRO_CODE_SIZE + range * 5] = CALL;

  /*
  ** Calculate relative offset for our call instruction
  ** then copy it in the code .
  */
  off = (int) new_routine -
    ((int) ((char *) &h->hook[INTRO_CODE_SIZE + 5 + range * 5]));
  *(int *) &h->hook[INTRO_CODE_SIZE + 1 + range * 5] = off;

  return (0);

}








/*
** DELETE A CALLBACK FOR THAT HOOK
** -> remove an entry , placing five NOP's (overwriting the call) 
** in the requested slot .
*/
int	khook_remove_entry(hook_t *h, int range)
{
  if (range < 0 && range > 7)
    return (-1);
  memset(&h->hook[INTRO_CODE_SIZE + range * 5], NOP, 5);
  return (0);
}




/*
** remove all callback for that hook
*/
void	khook_purge(hook_t *h)
{
  memset(&h->hook[INTRO_CODE_SIZE], NOP, 40);
}




/*
** Destroy a kernel hook
*/
void	khook_destroy(hook_t *h)
{
  khook_set_state(h, HOOK_DISABLED);
  kfree(h);
}






/*
** Create a kernel hook and set its attributes
**
** Hook the function putting an indirect jump instruction pointing to 
** the hook code .
** Since we have to overwrite the function 's code beginning , 7 bytes are
** saved .
**
** The hijacked function is still called, but after the hook code 
** Check the asmcode() function for human readable code ;)
**
** The hook can be set for functions with o without stack frame pointer
**
*/
hook_t		*khook_create(int hooked, int mask)
{
  int		tmp;
  hook_t	*h;
  char		hook_code[] = 

    
    "\xBE\x00\x00\x00\x00"	/* movl $0x00, %esi	*/
				/* 1ST RELOCATION (&saved_bytes[0], absolute) */
    "\xBF\x00\x00\x00\x00"	/* movl $0x00, %edi	*/
				/* 2ND RELOCATION (hijack_me absolute offset) */
    "\x1E"			/* push %ds		*/
    "\x07"			/* pop  %es		*/
    "\xFC"			/* cld			*/
    "\x31\xC9"			/* xor %ecx, %ecx	*/
    "\xB1\x07"			/* movl $0x07, %cl	*/
    "\xF3\xA4"			/* rep movsb		*/

    "\x8D\x44\x24\x08"		/* leal 8(%esp), %eax	*/
    "\x50"			/* push %eax		*/
    "\x90\x90\x90\x90\x90"	/* hook entry 0		*/
    "\x90\x90\x90\x90\x90"	/* hook entry 1		*/
    "\x90\x90\x90\x90\x90"	/* hook entry 2		*/
    "\x90\x90\x90\x90\x90"	/* hook entry 3		*/
    "\x90\x90\x90\x90\x90"	/* hook entry 4		*/
    "\x90\x90\x90\x90\x90"	/* hook entry 5		*/
    "\x90\x90\x90\x90\x90"	/* hook entry 6		*/
    "\x90\x90\x90\x90\x90"	/* hook entry 7		*/
    "\x83\xC4\x04"		/* addl $0x04, %esp	*/

    "\x4C"			/* dec %esp		*/
    "\xEB\x14"			/* jmp end		*/
    "\x5E"			/* begin: pop %esi	*/
    "\x8B\x44\x24\x05"		/* movl 5(%esp), %eax	*/
    "\x89\x46\x14"		/* movl %eax, 20(%esi)	*/
    "\x89\x74\x24\x05"		/* movl %esi, 5(%esp)	*/ 
    "\x44"			/* inc %esp		*/
    "\xB8\x00\x00\x00\x00"	/* movl $0x00, %eax	*/
				/* 3RD RELOCATION (hijack_me() + 3, absolute) */
    "\xFF\xE0"			/* jmp *%eax		*/
    "\xE8\xE7\xFF\xFF\xFF"	/* end: call begin	*/
    
    "\xBE\x00\x00\x00\x00"	/* movl $0x00, %esi	*/
				/* 4TH RELOCATION (&voodoo_bytes[0], absolute) */
    "\xBF\x00\x00\x00\x00"	/* movl $0x00, %edi */
				/* 5TH RELOCATION (hijack_me() + 3, absolute) */
    "\x1E"			/* push %ds		*/
    "\x07"			/* pop  %es		*/
    "\xFC"			/* cld			*/
    "\x31\xC9"			/* xor %ecx, %ecx	*/
    "\xB1\x07"			/* movl $0x07, %cl	*/
    "\xF3\xA4"			/* rep movsb		*/

    "\xB9\x00\x00\x00\x00"	/* movl $0x00, %ecx	*/
    "\x51"			/* pushl %ecx		*/
    "\xC3";			/* ret			*/





  /*
  ** Allocate space and resolve the symbol .
  */
  if ((h = (hook_t *) kmalloc(sizeof(hook_t), GFP_KERNEL)) == NULL)
    QUIT("kmalloc");
  
  if (!hooked)
    {
      kfree(h);
      QUIT("[DEBUG] kernel_hook_set: Cant resolve hijacked symbol .\n");
    }
  h->addr = hooked;


  /*
  ** Fill the hijacked_bytes field (aka constructing the indirect jump)
  */
  h->voodoo_bytes[0] = MOV_EAX;
  *(int *) &h->voodoo_bytes[1] = (int) h->hook;
  h->voodoo_bytes[5] = IJMP1;
  h->voodoo_bytes[6] = IJMP2;

  
  /*
  ** Guess if the hijacked function has a stack frame 
  */
  tmp = memcmp((char *) h->addr,
	       STACKFRAME_CONSTANT,
	       STACKFRAME_CONSTANT_SIZE);
  h->offset = (!tmp ? STACKFRAME_CONSTANT_SIZE : 0);

  
  /*
  ** If the function doesnt have a stackframe, patch three bytes
  ** in the hookcode .
  */
  if (!h->offset)
    {
      hook_code[FIXUP_ONE]   = 0x04;
      hook_code[FIXUP_TWO]   = 0x01;
      hook_code[FIXUP_THREE] = 0x01;
    }
  
  
  /*
  ** filling the saved_bytes field (aka saving the 7 overwritten bytes 
  ** of the function)
  */
  memcpy(h->saved_bytes, (void *) hooked + h->offset, sizeof(h->saved_bytes));


  /*
  ** setting the hook code and calculating relocation offsets .
  */
  memcpy(h->hook, hook_code, sizeof(hook_code));
  *(int *) &h->hook[REL_OFFSET_ONE]   = (int) h->saved_bytes;
  *(int *) &h->hook[REL_OFFSET_TWO]   = (int) h->addr + h->offset;
  *(int *) &h->hook[REL_OFFSET_THREE] = (int) h->addr + h->offset;
  *(int *) &h->hook[REL_OFFSET_FOUR]  = (int) h->voodoo_bytes;
  *(int *) &h->hook[REL_OFFSET_FIVE]  = (int) h->addr + h->offset;


  /*
  ** Creating the caches used for aggressive mode
  */
  memcpy(h->cache1, &h->hook[CACHE1_OFFSET], CACHE1_SIZE);
  memcpy(h->cache2, &h->hook[CACHE2_OFFSET], CACHE2_SIZE);


  /*
  ** Setting the hook parameters
  */
  if (khook_set_attr(h, mask) < 0)
    {
      kfree(h);
      QUIT("kernel_set_attr: invalid mask \n");
    }

  return (h);
}






/*
** Front end for khook_set_type, khook_set_state and khook_set_mode
*/
int	khook_set_attr(hook_t *h, int mask)
{
  if ((mask & HOOK_ENABLED) && (mask & HOOK_DISABLED))
    return (-1);
  if ((mask & HOOK_SINGLESHOT) && (mask & HOOK_PERMANENT))
    return (-1);
  if ((mask & HOOK_AGGRESSIVE) && (mask & HOOK_DISCRETE))
    return (-1);
  
  if (mask & HOOK_ENABLED)
    khook_set_state(h, HOOK_ENABLED);
  else if (mask & HOOK_DISABLED)
    khook_set_state(h, HOOK_DISABLED);

  if (mask & HOOK_SINGLESHOT)
    khook_set_type(h, HOOK_SINGLESHOT);
  else if (mask & HOOK_PERMANENT)
    khook_set_type(h, HOOK_PERMANENT);
  
  if (mask & HOOK_AGGRESSIVE)
    khook_set_mode(h, HOOK_AGGRESSIVE);
  else if (mask & HOOK_DISCRETE)
    khook_set_mode(h, HOOK_DISCRETE);

  return (0);
}






/*
** Change the hook type (HOOK_SINGLESHOT or HOOK_PERMANENT)
*/
int	khook_set_type(hook_t *h, char type)
{

  /* overwrite the 10 first byte with an indirect jump */
  if (type == HOOK_PERMANENT) 
    {
      memcpy(h->hook + HOOK_CONSTANT1_OFFSET,
	     HOOK_CONSTANT1,
	     HOOK_CONSTANT1_SIZE);
      return (0);
    }
  else if (type == HOOK_SINGLESHOT)
    {
      memset(h->hook + HOOK_CONSTANT1_OFFSET,
	     NOP,
	     HOOK_CONSTANT1_SIZE);
      return (0);
    }

  /* The requested type is invalid */
  return (-1);

}







/*
** Say whether or not the hijacked function will be called after
** the callback functions .
*/
int		khook_set_mode(hook_t *h, char mode)
{

  if (mode == HOOK_AGGRESSIVE)
    {
      memset(&h->hook[CACHE1_OFFSET], NOP, CACHE1_SIZE);
      memset(&h->hook[CACHE2_OFFSET], NOP, CACHE2_SIZE);
      if (h->offset)
	h->hook[CACHE2_OFFSET] = LEAVE;
      return (0);
    }
  else if (mode == HOOK_DISCRETE)
    {
      memcpy(&h->hook[CACHE1_OFFSET], h->cache1, CACHE1_SIZE);
      memcpy(&h->hook[CACHE2_OFFSET], h->cache2, CACHE2_SIZE);
      return (0);
    }

  /* Incorrect mode */
  return (-1);

}







/*
** Enable/Disable a hook (it's enabled by default)
*/
int	khook_set_state(hook_t *h, char state)
{

  /*
  ** Enable the hook
  */
  if (state == HOOK_ENABLED)
    {
      memcpy((char *) h->addr + h->offset,
	     h->voodoo_bytes,
	     sizeof(h->voodoo_bytes));
      return (0);
    }

  /*
  ** Disable the hook
  */
  else if (state == HOOK_DISABLED)
    {
      memcpy((char *) h->addr + h->offset,
	     h->saved_bytes,
	     sizeof(h->saved_bytes));	
      return (0);
    }

  /*
  ** The requested state is invalid 
  */
  return (-1);

}







#if 0

/* 
** Get the kernel symbol value for this name
*/
int				ksym_lookup(char *name)
{
  static struct kernel_sym	*list = NULL;
  static int			lenght;
  int				index;
  int				(*get_symlist)(struct kernel_sym *l);
  
  if (!list)
    {
      get_symlist = sys_call_table[__NR_get_kernel_syms];
      lenght = get_symlist(NULL);
      list = kmalloc(lenght * sizeof(struct kernel_sym), GFP_KERNEL);
      if (!list)
	return (-1);
      set_fs(KERNEL_DS);
      get_symlist(list);
      set_fs(USER_DS);
    }

  for (index = 0; index < lenght; index++)
    if (!strcmp(list[index].name, name))
      return (list[index].value);

  return (0);
}

#endif














