#!/bin/sh

rm -rf ./brn/src
mkdir -p ./brn/src

#(cd ../../../../; make install)

for i in `ls ../..`; do
    if [ "x$i" != "xtools" ]; then
       echo $i
       cp -r ../../$i ./brn/src 
    fi 
done

./configure --enable-userlevel --with-click=$CLICKPATH/click_install