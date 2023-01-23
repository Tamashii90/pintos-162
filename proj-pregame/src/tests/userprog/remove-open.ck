# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(remove-open) begin
(remove-open) create remove-test
(remove-open) write "1234" to remove-test
(remove-open) remove remove-test
(remove-open) read "1234" from remove-test
(remove-open) end
remove-open: exit(0)
EOF
pass;
