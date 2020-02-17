/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include "opt-A2.h"
#include <copyinout.h>

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char **args, unsigned long nargs)
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

#if OPT_A2 // enabling argument passing for runprogram
	size_t actual; // I have no idea what this shit does
    int total_arg_size = 0;
    //int num_args = 0;
    // Count the number of arguments
    int num_args = nargs;

    // check for limit
    if (num_args > ARG_MAX) {
      return E2BIG;
    }

    for (int i = 0; i < num_args; i++) {
    	int size = strlen(args[i]) + 1;
    	if (size % 4) {
    		size = ROUNDUP(size, 4);
    	}
    	total_arg_size += size;
    }

#endif // OPT_A2

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	// Pop the current as
	 /* Destroy the current process's address space to create a new one. */
    /* as = curproc->p_addrspace;
    as_destroy(as);
    curproc->p_addrspace = NULL; */

	/* We should be a new process. */
	KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

#if OPT_A2 // put args onto the user stack
	// Array of address of the args, to be passed onto the user as
  char **user_args_addr = kmalloc(sizeof(char*) * num_args + 1);
  user_args_addr[num_args] = NULL;

  // Copy arguments into user stack
  stackptr -= total_arg_size;
  for (int i = 0; i < num_args; i++) {
    int size = strlen(args[i]) + 1;
    if (size % 4) {
      size = ROUNDUP(size, 4);
    } 
    user_args_addr[i] = (char *)stackptr;
    copyoutstr(args[i], (userptr_t)stackptr, size, &actual);
    stackptr += size;
  }
  // get space for the array of address
  stackptr = stackptr - total_arg_size - (sizeof(char*) * (1 + num_args));

  for (int i = 0; i < num_args + 1; i++) {
      copyout(user_args_addr + i, (userptr_t)stackptr, sizeof(char *));
      stackptr += sizeof(char *);
  }
  // reset stackptr
  stackptr -= sizeof(char*) * (1 + num_args);

  // cleanup
  kfree(user_args_addr);
 

  /* Warp to user mode. */
  enter_new_process(num_args /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
        stackptr, entrypoint);

#endif //OPT_A2

	/* Warp to user mode. */
	//enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
	//		  stackptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

