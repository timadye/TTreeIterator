#!/bin/bash
if [ -n "${1%-*}" ]; then
  prog="$1"; shift
else
  prog="test1"
fi
set -x
$(root-config --cxx --cflags --ldflags --libs) -I$(dirname $(readlink -f "$0")) -o "$prog" "$prog.cxx" -DNO_DICT=1 "$@"
