#!/bin/sh

if [ x"$1" == x"" ]; then
    echo "Need start revision"
    exit 1
fi

if [ x"$2" == x"" ]; then
    echo "Need end revision"
    exit 1
fi

echo "Report for changes from $1 to $2"
echo
echo "Change short log: "
git shortlog $1..$2 | grep -v "Merge"
echo

echo "Branches merged: "
git shortlog $1..$2 | grep "Merge"
echo

echo "Change summary: "
git diff $1..$2 | diffstat -p1
