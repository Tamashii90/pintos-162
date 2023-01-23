# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(remove-normal) begin
(remove-normal) create remove-test
(remove-normal) remove remove-test
(remove-normal) end
remove-normal: exit(0)
EOF
pass;
