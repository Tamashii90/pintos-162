/* Remove a file. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void test_main(void) {
  CHECK(create("remove-test", 0), "create remove-test");
  CHECK(remove("remove-test"), "remove remove-test");
  int handle = open("remove-test");
  if (handle != -1)
    fail("opening removed file returned %d instead of -1", handle);
}
