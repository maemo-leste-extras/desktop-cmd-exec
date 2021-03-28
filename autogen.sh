#!/bin/sh

set -x
aclocal
autoconf
libtoolize
automake --add-missing --foreign
