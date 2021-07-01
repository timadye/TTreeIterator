#!/bin/bash
base=$(basename "$0" .sh)
dir=$(dirname $(readlink -e "$0" | sed 's=^/net/home/=/home/='))
if [ -n "$1" ]; then
  n="$1"
  shift
else
  n=1
fi
if [ -n "$1" ]; then
  csv="$1"
  shift
else
  csv=$base.csv
fi
defs=("$@")
[ -z "$LABEL" ] && LABEL="$base"
#rm -f "$csv"

run() {
  echo + "$@"
  "$@"
}

c() {
  run ./maketiming2.sh -DNO_BranchInfo_STATS=1 -DFAST_CHECKS=1 "${defs[@]}" "$@"
}

t() {
  run env TIMELOG="$csv" PAD="$(printf "%$(($RANDOM % 4096))s" '' | tr ' ' .)" ./TestTiming --gtest_filter="$1"
}

set -e
run ./make.sh
[ -z "$STABILIZER" ] && . "$dir/../../../stabilizer/setup-base.sh"
c
set +e
run ./TestTiming --gtest_filter="timingTests1.FillIter"

for i in $(seq $n); do
  echo ========== Test $i of $n ========== 
  t 'timingTests1.GetIter'
done

run "$dir/plotTimes.py" "$csv"
