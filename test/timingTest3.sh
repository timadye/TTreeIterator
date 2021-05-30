#!/bin/bash
if [ -n "$1" ]; then
  csv="$1"
  shift
else
  csv=$(basename "$0" .sh).csv
fi
defs=("$@")
rm -f "$csv"

run() {
  echo + "$@"
  "$@"
}

c() {
  run ./maketiming.sh -DNO_BranchInfo_STATS=1 -DFAST_CHECKS=1 "${defs[@]}" "$@"
}

t() {
  run env LABEL="$1" TIMELOG="$csv" ./TestTiming --gtest_filter="$2"
}

set -e
run ./make.sh
c
run ./TestTiming --gtest_filter="timingTests3.FillAddr"

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
