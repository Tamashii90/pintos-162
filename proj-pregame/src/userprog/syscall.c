#include "userprog/syscall.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "list.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"

static void syscall_handler(struct intr_frame*);

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }

void sys_exit(int status) {
  struct thread* cur = thread_current();
  struct thread* parent = cur->parent_thread;
  printf("%s: exit(%d)\n", cur->name, status);

  struct child* me = child_find(parent->children, cur->tid);
  me->exit_code = status;
  sema_up(parent->sema_exit);

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

static void validate_file_name(char* name) {
  uint32_t* pd = thread_current()->pcb->pagedir;
  char* temp = name;
  size_t i = 0;
  while (true) {
    if (i == 255 || temp == NULL || !is_user_vaddr(temp) || pagedir_get_page(pd, temp) == NULL)
      sys_exit(-1);
    if (*temp == '\0')
      break;
    i++;
    temp++;
  }
}

static void validate_fd(int fd) {
  int curr_fd = thread_current()->pcb->curr_fd;
  if (fd < 0 || fd >= curr_fd) {
    sys_exit(-1);
  }
}

static int sys_write(int fd, void* buffer, unsigned length) {
  lock_acquire(&file_lock);
  struct process* pcb = thread_current()->pcb;
  struct file* file = pcb->open_files[fd];
  int res = -99;
  if (fd == STDIN_FILENO) {
    lock_release(&file_lock);
    sys_exit(-1);
  }
  if (fd == STDOUT_FILENO) {
    putbuf(buffer, length);
    lock_release(&file_lock);
    return length;
  }
  res = file_write(file, buffer, length);
  lock_release(&file_lock);
  return res;
}

static int sys_read(int fd, void* buffer, unsigned size) {
  lock_acquire(&file_lock);
  struct process* pcb = thread_current()->pcb;
  struct file* file = pcb->open_files[fd];
  int res = -99;
  if (fd == STDOUT_FILENO || file == NULL) {
    lock_release(&file_lock);
    return -1;
  }
  if (fd == STDIN_FILENO) {
    char input;
    for (size_t i = 0; i < size; i++) {
      input = input_getc();
      ((char*)buffer)[i] = input;
    }
    lock_release(&file_lock);
    return size;
  }
  res = file_read(file, buffer, size);
  lock_release(&file_lock);
  return res;
}

static int sys_open(char* file_name) {
  lock_acquire(&file_lock);
  struct process* pcb = thread_current()->pcb;
  struct file* file = filesys_open(file_name);
  int res = -99;
  if (pcb->curr_fd == MAX_FILES || file == NULL) {
    lock_release(&file_lock);
    return -1;
  }
  pcb->open_files[pcb->curr_fd] = file;
  res = pcb->curr_fd++;
  lock_release(&file_lock);
  return res;
}

void sys_close(int fd) {
  lock_acquire(&file_lock);
  struct process* pcb = thread_current()->pcb;
  struct file* file = pcb->open_files[fd];
  // No closing process's exe, STDOUT, or STDIN
  if (fd < 3 || file == NULL) {
    lock_release(&file_lock);
    sys_exit(-1);
  }
  file_close(file);
  pcb->open_files[fd] = NULL;
  lock_release(&file_lock);
}

static int sys_filesize(int fd) {
  lock_acquire(&file_lock);
  struct process* pcb = thread_current()->pcb;
  struct file* file = pcb->open_files[fd];
  int res = -99;
  if (file == NULL) {
    lock_release(&file_lock);
    return -1;
  }
  res = file_length(file);
  lock_release(&file_lock);
  return res;
}

static void sys_seek(int fd, unsigned position) {
  lock_acquire(&file_lock);
  struct process* pcb = thread_current()->pcb;
  struct file* file = pcb->open_files[fd];
  if (file == NULL || fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    lock_release(&file_lock);
    sys_exit(-1);
  }
  file_seek(file, position);
  lock_release(&file_lock);
}

static unsigned sys_tell(int fd) {
  lock_acquire(&file_lock);
  struct process* pcb = thread_current()->pcb;
  struct file* file = pcb->open_files[fd];
  int res = -99;
  if (file == NULL || fd == STDIN_FILENO || fd == STDOUT_FILENO) {
    lock_release(&file_lock);
    sys_exit(-1);
  }
  res = file_tell(file);
  lock_release(&file_lock);
  return res;
}

static int sys_practice(int val) { return val + 1; }

static bool sys_create(char* file_name, unsigned initial_size) {
  lock_acquire(&file_lock);
  int res = false;
  res = filesys_create(file_name, initial_size);
  lock_release(&file_lock);
  return res;
}

static bool sys_remove(const char* file) {
  lock_acquire(&file_lock);
  int res = false;
  res = filesys_remove(file);
  lock_release(&file_lock);
  return res;
}

static pid_t sys_exec(const char* cmd_line) { return process_execute(cmd_line); }
static int sys_wait(pid_t child_pid) { return child_pid == -1 ? -1 : process_wait(child_pid); }

static void syscall_handler(struct intr_frame* f) {
  /* Current syscalls have 4 args max */
  validate_addr(f->esp, sizeof(int) * 4);

  uint32_t* args = ((uint32_t*)f->esp);
  uint32_t call_nr = args[0];

  switch (call_nr) {
    case SYS_EXIT: {
      // validate_addr(&args[1], sizeof(uint32_t));
      sys_exit((int)args[1]);
      break;
    }

    case SYS_HALT: {
      shutdown();
      NOT_REACHED();
      break;
    }

    case SYS_PRACTICE: {
      // validate_addr(&args[1], sizeof(int));
      f->eax = sys_practice((int)args[1]);
      break;
    }

    case SYS_EXEC: {
      validate_file_name((char*)args[1]);
      f->eax = (uint32_t)sys_exec((char*)args[1]);
      break;
    }

    case SYS_WAIT: {
      // validate_addr(&args[1], sizeof(int));
      f->eax = (uint32_t)sys_wait((pid_t)args[1]);
      break;
    }

    case SYS_CREATE: {
      validate_file_name((char*)args[1]);
      f->eax = (uint32_t)sys_create((char*)args[1], (unsigned)args[2]);
      break;
    }

    case SYS_REMOVE: {
      validate_file_name((char*)args[1]);
      f->eax = (uint32_t)sys_remove((char*)args[1]);
      break;
    }

    case SYS_OPEN: {
      validate_file_name((char*)args[1]);
      f->eax = (uint32_t)sys_open((char*)args[1]);
      break;
    }

    case SYS_FILESIZE: {
      // validate_addr(&args[1], sizeof(int));
      validate_fd((int)args[1]);
      f->eax = (uint32_t)sys_filesize((int)args[1]);
      break;
    }

    case SYS_SEEK: {
      validate_fd((int)args[1]);
      sys_seek((int)args[1], (unsigned)args[2]);
      break;
    }

    case SYS_TELL: {
      validate_fd((int)args[1]);
      f->eax = (uint32_t)sys_tell((int)args[1]);
      break;
    }

    case SYS_READ: {
      validate_fd((int)args[1]);
      validate_addr((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)sys_read((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;
    }

    case SYS_WRITE: {
      validate_fd((int)args[1]);
      validate_addr((void*)args[2], (unsigned)args[3]);
      f->eax = (uint32_t)sys_write((int)args[1], (void*)args[2], (unsigned)args[3]);
      break;
    }

    case SYS_CLOSE: {
      validate_fd((int)args[1]);
      sys_close(args[1]);
      break;
    }

    default:
      printf("No such system call [%d]\n", call_nr);
  }
}
