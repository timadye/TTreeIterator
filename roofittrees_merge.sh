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

start=$(git log --pretty=format:%H,%ad --date=short | sed -n 's/,2021-06-25$//p')
prev=$(git log -n1 --pretty=format:%H "$start"^)
base=$(echo "base for updates imported from https://github.com/timadye/TTreeIterator" | git commit-tree "$prev"^{tree})
git rebase --onto "$base" "$prev"

git remote add origin ssh://git@gitlab.cern.ch:7999/will/roofittrees.git
git fetch origin tim
git checkout tim

orig=$(git log --pretty=format:%H,%ad --date=short | sed -n 's/,2021-06-25$//p')

git merge --no-commit --allow-unrelated-histories -s recursive -X theirs main
git commit -m "merge changes from https://github.com/timadye/TTreeIterator"

newstart=$(git log --pretty=format:%H,%ad --date=short main | sed -n 's/,2021-06-25$//p')

# join history back when they were split apart
git replace "$newstart" "$orig"

git branch -d main

# unfortunately this replacement doesn't get pushed to the origin.
# The following might help with that, but we then lose the even earlier history after we branched from origin/master.
#git filter-repo --force
#git remote add origin ssh://git@gitlab.cern.ch:7999/will/roofittrees.git

ln -s /scratch/adye/$dir scratch
cp -pi ~/dev/TTreeIterator/setup-100.sh .
