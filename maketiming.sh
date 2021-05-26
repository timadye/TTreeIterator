#!/bin/bash
MAKE_SH=$(readlink -f "$0" | sed 's=^/net/home/=/home/=')
[ -z "$SETUP_SH"   ] && export SETUP_SH=$(dirname "$MAKE_SH")/setup.sh
[ -z "$BUILD_DIR"  ] && export BUILD_DIR=$(dirname "$SETUP_SH")/build
SRCDIR="$(dirname $(readlink -f "$0" | sed 's=^/net/home/=/home/='))"
cd "$BUILD_DIR/TTreeIterator"
make TestTiming MAKESILENT="-W$SRCDIR/TTreeIterator/test/timingTests.cxx" CXX_DEFINES="$*"
