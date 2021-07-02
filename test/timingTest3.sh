#!/bin/bash
base=$(basename "$0" .sh)
dir=$(dirname $(readlink -e "$0" | sed 's=^/net/home/=/home/='))
if [ $# -ge 1 ]; then
  n="$1"
  shift
fi
[ -z "$n" ] && n=1
if [ $# -ge 1 ]; then
  csv="$1"
  shift
fi
[ -z "$csv" ] && csv="$base.csv"
defs=("$@")
rm -f "$csv"

run() {
  echo + "$@"
  "$@"
}

c() {
  run ./maketiming2.sh -DNO_BranchInfo_STATS=1 -DFAST_CHECKS=1 "${defs[@]}" "$@"
}

tt() {
  run env LABEL="$1" TIMELOG="$csv" PAD="$(printf "%$(($RANDOM % 4096))s" '' | tr ' ' .)" ./TestTiming --gtest_filter="$2"
}

t() {
for i in $(seq $n); do
  echo "==================== Test $1 $2 - #$i of $n ===================="
  tt "$@"
done
}

set -e
run ./make.sh
c
run ./TestTiming --gtest_filter="timingTests3.FillIter"
set +e

                                                 t 'SetBranchAddress' 'timingTests3.GetAddr'
                                                 t 'TTreeReaderArray' 'timingTests3.GetReader'
                                                 t 'TTreeReaderValue' 'timingTests3.GetReader2'
c -DUSE_std_any=1 -DUSE_map=1                  ; t 'map'              'timingTests3.GetIter'
c -DUSE_std_any=1 -DUSE_OrderedMap             ; t 'map+vector'       'timingTests3.GetIter'
c -DUSE_std_any=1                              ; t 'vector'           'timingTests3.GetIter'
c                                              ; t 'std::any opt'     'timingTests3.GetIter'
c -DFEWER_CHECKS=1 -DOVERRIDE_BRANCH_ADDRESS=1 ; t 'no checks'        'timingTests3.GetIter'

run ./maketiming.sh
run $(dirname "$0")/plotTimes.py "$csv"
