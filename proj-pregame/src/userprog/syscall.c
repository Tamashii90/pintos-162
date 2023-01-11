#include "userprog/syscall.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

void sys_exit(int status) {
  printf("%s: exit(%d)\n", thread_current()->pcb->process_name, status);
  process_exit();
}

/*
 * Must handle three cases:
 * 1) addr is null
 * 2) addr is not a user virtual address
 * 3) addr is not a mapped user virtual address
 */
static void validate_addr(void* addr, unsigned size) {
  uint32_t* pd = thread_current()->pcb->pagedir;
  void* temp = addr;
  for (size_t i = 0; i < size; i++, temp++) {
    if (temp == NULL || !is_user_vaddr(temp) || pagedir_get_page(pd, temp) == NULL)
      sys_exit(-1);
  }
}

static int sys_write(int fd, void* buffer, unsigned length) {
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, length);
    return length;
  }
  return -1;
}

static int sys_practice(int val) { return val + 1; }

static void syscall_handler(struct intr_frame* f) {
  uint32_t* args = ((uint32_t*)f->esp);
  uint32_t call_nr = args[0];

  switch (call_nr) {
    case SYS_EXIT: {
      validate_addr(&args[1], sizeof(uint32_t));
      sys_exit((int)args[1]);
      break;
    }
    case SYS_HALT: {
      shutdown();
      NOT_REACHED();
      break;
    }

    case SYS_WRITE: {
      validate_addr((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)sys_write((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;
    }

    case SYS_PRACTICE: {
      validate_addr(&args[1], sizeof(int));
      f->eax = sys_practice((int)args[1]);
      break;
    }

    default:
      printf("No such system call [%d]\n", call_nr);
  }
}
