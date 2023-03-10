#!/usr/bin/env bash
set -uo pipefail
files_to_re_add=$(mktemp)

if comm -23 <(git diff --cached --name-only --diff-filter=CMRA | sort)\
            <(git diff --name-only --diff-filter=CMRA | sort)\
            | grep '\.[ch]$' > "$files_to_re_add"
then
  ( cd "$(git rev-parse --show-toplevel)/$(git rev-parse --show-prefix)" && make format )
	xargs git add < "$files_to_re_add"
fi

rm "$files_to_re_add"
