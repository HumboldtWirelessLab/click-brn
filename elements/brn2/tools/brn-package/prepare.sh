#!/bin/sh

rm -rf ./brn/src
mkdir -p ./brn/src

for i in `ls ../..`; do
    if [ "x$i" != "xtools" ]; then
       echo $i
       cp -r ../../$i ./src 
    fi 
done
