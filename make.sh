#!/bin/bash
MAKE_SH=$(readlink -f "$0" | sed 's=^/net/home/=/home/=')
[ -z "$SETUP_SH"   ] && export SETUP_SH=$(dirname "$MAKE_SH")/setup.sh
[ -z "$RUNDIR"     ] && export RUNDIR=$(dirname "$SETUP_SH")/run
[ -z "$BUILD_DIR"  ] && export BUILD_DIR=$(dirname "$SETUP_SH")/build
if [ -n "$1" ]; then
  export BUILD_TYPE="$1"
  shift
elif [ -z "$BUILD_TYPE" ]; then
  export BUILD_TYPE=Release
fi
SRCDIR="$(dirname $(readlink -f "$0" | sed 's=^/net/home/=/home/='))"
cd "$SRCDIR"
mkdir -p "$BUILD_DIR" "$RUNDIR"
[ -d $(basename "$BUILD_DIR") ] || ln -nfs "$BUILD_DIR" .
[ -d $(basename "$RUNDIR")    ] || ln -nfs "$RUNDIR"    .
cd "$BUILD_DIR"
cmake3 "$SRCDIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_INSTALL_PREFIX="$RUNDIR" -DCMAKE_INSTALL_MESSAGE=LAZY "$@" || exit
make install || exit
cd "$RUNDIR"
ln -nfs "$BUILD_DIR/TTreeIterator/Test"* "$BUILD_DIR/TTreeIterator/Bench"* "$SRCDIR/TTreeIterator/test/"*.sh "$SRCDIR/TTreeIterator/test/"*.py $(dirname "$MAKE_SH")/make*.sh .
ln -nfs "$SETUP_SH" setup.sh
