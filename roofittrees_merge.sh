#!/bin/bash
dir="roofittrees.$$"
set -x
git clone git@github.com:timadye/TTreeIterator.git $dir
cd $dir
git filter-repo \
    --path TTreeIterator \
    --path test \
    --path-glob 'make*.sh' \
    --path-rename TTreeIterator:RooFitTrees/RooFitTrees \
    --path-rename test:RooFitTrees/test \
    --replace-text <(echo "TTreeIterator/==>RooFitTrees/")
git filter-repo \
    --path README.md \
    --path CMakeLists.txt \
    --path-glob 'RooFitTrees/test/*.root' \
    --invert-paths

git remote add origin ssh://git@gitlab.cern.ch:7999/will/roofittrees.git
git fetch origin tim
git checkout tim
git merge --no-commit --allow-unrelated-histories -s recursive -X theirs main
git commit -m "merge changes from https://github.com/timadye/TTreeIterator"

ln -s /scratch/adye/$dir scratch
cp -pi ~/dev/TTreeIterator/setup-100.sh .
