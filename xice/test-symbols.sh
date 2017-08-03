#!/bin/sh

set -e

if test -z "$srcdir"; then
	srcdir=.
fi

chmod +x $srcdir/../scripts/*.sh
check_symbols=$srcdir/../scripts/check-symbols.sh

if ! test -f $check_symbols; then
	echo "cannot find check-symbols.sh"
	exit 1
fi

if ! test -f .libs/libxice.so; then
	echo "no shared object found" >&2
	exit 77
fi

sh $check_symbols .libs/libxice.so libxice.symbols
