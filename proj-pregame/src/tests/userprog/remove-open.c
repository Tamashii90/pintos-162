/* Remove a file. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void) {
  const char* buffer = "1234";
  int size = 4;
  char input[size + 1];

  CHECK(create("remove-test", size), "create remove-test");

  int handle = open("remove-test");
  int handle2 = open("remove-test");
  if (handle == -1 || handle2 == -1) {
    fail("Cannot open file");
  }

  CHECK(write(handle, buffer, size) == size, "write \"%s\" to remove-test", buffer);
  CHECK(remove("remove-test"), "remove remove-test");

  read(handle2, input, size);
  input[size + 1] = '\0';
  msg("read \"%s\" from remove-test", input);
}
