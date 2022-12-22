/*
 * Word count application with one thread per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */

/*
 * Copyright © 2021 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "word_count.h"
#include "word_helpers.h"

typedef struct thread_argument {
  char* file_name;
  word_count_list_t* word_counts;
} thread_argument_t;

void* func(void*);

/*
 * main - handle command line, spawning one thread per file.
 */
int main(int argc, char* argv[]) {
  /* Create the empty data structure. */
  word_count_list_t word_counts;
  init_words(&word_counts);

  if (argc <= 1) {
    /* Process stdin in a single thread. */
    count_words(&word_counts, stdin);
    wordcount_sort(&word_counts, less_count);
    fprint_words(&word_counts, stdout);
    return 0;
  }
  /* threads will start at threads[1] for convenience (same as argc) */
  pthread_t threads[argc];
  for (size_t t = 1; t < argc; t++) {
    thread_argument_t* thread_arg = malloc(sizeof(thread_argument_t));
    thread_arg->file_name = argv[t];
    thread_arg->word_counts = &word_counts;

    int rc = pthread_create(&threads[t], NULL, func, (void*)thread_arg);
    if (rc) {
      printf("ERROR! Creating a thread failed with code: %d\n", rc);
      exit(1);
    }
  }

  /* Wait for all threads to finish */
  for (size_t t = 1; t < argc; t++) {
    int ret = pthread_join(threads[t], NULL);
    switch (ret) {
      case 0:
        printf("The thread joined successfully\n");
        break;
      case EDEADLK:
        printf("Deadlock detected\n");
        break;
      case EINVAL:
        printf("The thread is not joinable\n");
        break;
      case ESRCH:
        printf("No thread with given ID is found\n");
        break;
      default:
        printf("Error occurred when joining the thread\n");
    }
  }

  /* Output final result of all threads' work. */
  wordcount_sort(&word_counts, less_count);
  fprint_words(&word_counts, stdout);
  return 0;
}

void* func(void* thread_arg) {
  thread_argument_t* thread_arg_t = (thread_argument_t*)thread_arg;
  word_count_list_t* word_counts = thread_arg_t->word_counts;
  char* file_name = thread_arg_t->file_name;

  FILE* fd = fopen(file_name, "r");
  if (fd == NULL) {
    if (errno == 2) {
      printf("File not found: %s\n", file_name);
    } else {
      printf("Error opening file %s errno: (%d)\n", file_name, errno);
    }
    free(thread_arg_t);
    pthread_exit(NULL);
  }

  count_words(word_counts, fd);

  fclose(fd);
  free(thread_arg_t);
  pthread_exit(NULL);
}

