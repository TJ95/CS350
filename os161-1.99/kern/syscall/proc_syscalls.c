#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include <vm.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include "opt-A2.h"

#if OPT_A2
/*
 * Handler for fork() syscall, fork creates a copy of the current
 * process and return 0 for the child process, and the PID of the
 * child process for the parent.
 */
int sys_fork(struct trapframe *tf, int* retval) 
{
  
  // Create process structure for child process and assign PID
  // and create parent/child relationship in runprogram
  struct proc *child_proc = proc_create_runprogram("child");
  KASSERT(child_proc != NULL);
  *retval = child_proc->pid;  

  // Create and copy address space
  struct addrspace *as_new;
  as_copy(curproc_getas(),&as_new);
  KASSERT(as_new != NULL);

  // Link the new address space to the new process
  spinlock_acquire(&child_proc->p_lock);
  child_proc->p_addrspace = as_new;
  spinlock_release(&child_proc->p_lock);

  // Create thread for child process
  int temp;
  struct trapframe *tfcopy = kmalloc(sizeof(struct trapframe));
  memcpy(tfcopy, tf, sizeof(struct trapframe));
  temp = thread_fork("some_thread",child_proc,enter_forked_process,(void *)tfcopy, 0);
  return(0);  

}

#endif /* OPT_A2 */

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

#if OPT_A2

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  as = curproc_setas(NULL);
  as_destroy(as);
  p->exitcode = _MKWAIT_EXIT(exitcode);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* curproc becomes zombie (as popped) */

  // locking
  lock_acquire(waitlk);
  
  // Clearing zombie children
  int size = p->num_child;
  struct proc *kid;
  for (int i = 0; i < size; i++) {
    kid = p->children[i];
    if (kid->p_addrspace == NULL) { // kid is a zombie
      proc_destroy(kid);
    }
  }
  // If p is an orphan, destroy
  if (p->parent == NULL) {
    Procs[p->pid] = NULL;
    proc_destroy(p);
  } else { // if p has a parent, signal waiting parent
    cv_signal(p->procv, waitlk);
  }
  
  //v_broadcast(p->procv, waitlk);
  //proc_destroy(p);
  // unlocking
  lock_release(waitlk);

 #endif /* OPT_A2 */ 
  thread_exit();
/* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
  
  *retval = curproc->pid;
  return(0);

#endif /* OPT_A2 */
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  if (options != 0) {
    return(EINVAL);
  }
  
#if OPT_A2

  // initialize variables
  int exitstatus;
  int result;
  struct proc *cproc = Procs[pid];
 
 if (cproc == NULL) { // such proc does not exist
    return ESRCH;
 }

 if (cproc->parent != curproc) { // proc called on is not interested
  return ECHILD;
 }

 // locking
 lock_acquire(waitlk);

 while (cproc->p_addrspace != NULL) {
  cv_wait(cproc->procv, waitlk);
 }

 // unlocking
 lock_release(waitlk);

 exitstatus = cproc->exitcode;
 proc_destroy(cproc);


 result = copyout((void *)&exitstatus,status,sizeof(int));
 if (result) {
    return(result);
 }
 *retval = pid;
 return(0);

#else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
#endif /* OPT_A2 */
}

#if OPT_A2

/* execv replaces currently executing program with a newly loaded program image. Process id remains unchanged.
 * Path of the program is passed in as progname. Arguments to the program args is an array of NULL terminated
 * strings. The array is terminated by a NULL ptr. In the new usr program, argv[argc] == NULL.
 */
int
sys_execv(char *progname, char ** args)
{

  if (progname == NULL) {
    return EFAULT;
  }

  struct addrspace *as;
  struct vnode *v;
  vaddr_t entrypoint, stackptr;
  int result;
  size_t actual; // I have no idea what this shit does
  int total_arg_size = 0;

  // Count the number of arguments
  int num_args = 0;
  while (args[num_args] != NULL) {
    num_args++;
  }
  //kprintf("number of argument is: %d \n", num_args);

  // check for limit
  if (num_args > ARG_MAX) {
        return E2BIG;
  }

  // Copy args into the kernel
  char **kargs = kmalloc(sizeof(char*) * num_args);
  for (int i = 0; i < num_args; i++) {
    int size = strlen(args[i]) + 1; // did not forget the \n
    if (size % 4) { 
      total_arg_size += ROUNDUP(size,4);
    } else {
      total_arg_size += size;
    }
    kargs[i] = kmalloc(sizeof(char*) * size);
    result = copyinstr((userptr_t)args[i], kargs[i], ARG_MAX, &actual);
  }

  // Copy progname into the kernel
  char *pname = kmalloc(sizeof(char) * NAME_MAX);
  result = copyinstr((userptr_t)progname, pname, NAME_MAX, &actual);

  if (result) {
      kfree(pname);
      return result;
  }

  /* ------------ runprogram cpy ----------- */
  /* Open the file. */
  result = vfs_open(pname, O_RDONLY, 0, &v);
  if (result) {
    kfree(pname);
    return result;
  }

  // Pop the current as
  as = curproc->p_addrspace;
  curproc->p_addrspace = NULL;
  as_destroy(as);

  /* We should be a new process. */
  KASSERT(curproc_getas() == NULL);

  /* Create a new address space. */
  as = as_create();
  if (as ==NULL) {
    vfs_close(v);
    kfree(pname);
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
    kfree(pname);
    return result;
  }

  /* Done with the file now. */
  vfs_close(v);
  /* ------------------------SETTING UP THE AS---------------------------*/
  /* Define the user stack in the address space */
  result = as_define_stack(as, &stackptr);
  if (result) {
    /* p_addrspace will go away when curproc is destroyed */
    kfree(pname);
    return result;
  }

  // Array of address of the args, to be passed onto the user as
  char **user_args_addr = kmalloc(sizeof(char*) * num_args + 1);
  user_args_addr[num_args] = NULL;

  // Copy arguments into user stack
  stackptr -= total_arg_size;
  for (int i = 0; i < num_args; i++) {
    int size = strlen(kargs[i]) + 1;
    if (size % 4) {
      size = ROUNDUP(size, 4);
    } 
    user_args_addr[i] = (char *)stackptr;
    copyoutstr(kargs[i], (userptr_t)stackptr, size, &actual);
    stackptr += size;
  }
  // get space for the array of ptrs to the args
  stackptr = stackptr - total_arg_size - (sizeof(char*) * (1 + num_args));
  for (int i = 0; i < num_args + 1; i++) {
      copyout(user_args_addr + i, (userptr_t)stackptr, sizeof(char *));
      stackptr += sizeof(char *);
  }
  // reset stackptr
  stackptr -= sizeof(char*) * (1 + num_args);

  // cleanup
  kfree(pname);
  kfree(user_args_addr);
  for (int i = 0; i < num_args; i++) {
    kfree(kargs[i]);
  }
  kfree(kargs);

  /* Warp to user mode. */
  enter_new_process(num_args /*argc*/, (userptr_t)stackptr /*userspace addr of argv*/,
        stackptr, entrypoint);
  /* ------------------------SETTING UP THE AS---------------------------*/ 
  /* enter_new_process does not return. */
  panic("enter_newg_process returned\n");
  return EINVAL;
}

#endif // OPT_A2

