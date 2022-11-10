#!/bin/sh
set -e
eval v=\${$1+x}
[ -z "$v" ] && exit 2
eval v=\${$1}
[ "$v" = "$2" ] && exit 0
exit 1
