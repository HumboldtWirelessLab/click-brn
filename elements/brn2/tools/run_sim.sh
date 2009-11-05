#!/bin/bash

SEDARG=""

for i in `ls *.click`; do
  SEDARG="$SEGARG -e s#$i#$i.tmp#g"
  DEBUG=$DEBUG click_dbg.sh $i $i.tmp
done

if [ "x$1" = "x" ]; then
  TCLFILE=`ls *.tcl | tail -n 1 | awk '{print $1}'`
else
  TCLFILE=$1
fi

cat $TCLFILE | sed $SEDARG > $TCLFILE.tmp

ns $TCLFILE.tmp

rm $TCLFILE.tmp
for i in `ls *.click`; do
  rm $i.tmp
done
