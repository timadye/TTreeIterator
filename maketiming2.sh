#!/bin/bash
PROG=timingTests
EXE=TestTiming
OFLAG="2" SZC=0 SZCFLAGS=() OPTS=()
for o in "$@"; do
  case "$o" in
    -Rcode|-Rheap|-Rstack|-Rlink|-fopt=*) SZCFLAGS+=("$o");;
    -szc) SZC=1;;
    -O*) OFLAG="${o#-O}";;
    *) OPTS+=("$o");;
  esac
done
OPTS+=("-O$OFLAG")
      
SRCDIR="$(dirname $(readlink -f "$0" | sed 's=^/net/home/=/home/='))"
if [ $SZC -ne 0 -o "${#SZCFLAGS[@]}" -gt 0 ]; then
  shift
  [ "${#SZCFLAGS[@]}" -le 0 ] && SZCFLAGS=(-Rheap -Rstack -Rlink)
  CXX="szc -lang=c++ -v -fopt=$OFLAG ${SZCFLAGS[*]}"
else
  CXX=$(root-config --cxx)
fi

EXEOUT=$(readlink -e "$EXE")
[ -z "$EXEOUT" ] && EXEOUT=$EXE

CXXFLAGS="$(root-config --cflags) -I$SRCDIR/TTreeIterator -DNO_DICT=1"
LDFLAGS="$(root-config --ldflags --libs) -O2 -L$BUILD_DIR/lib -lgtest"

set -x
mkdir -p obj
rm -f $EXEOUT obj/$PROG
$CXX -c -o obj/$PROG.o      "$SRCDIR/TTreeIterator/test/$PROG.cxx"                         $CXXFLAGS "${OPTS[@]}" || exit
$CXX -c -o obj/gtest_main.o "$BUILD_DIR/_deps/googletest-src/googletest/src/gtest_main.cc" $CXXFLAGS "${OPTS[@]}" || exit
$CXX    -o obj/$PROG        obj/gtest_main.o obj/$PROG.o                                   $LDFLAGS               || exit
mv obj/$PROG $EXEOUT
