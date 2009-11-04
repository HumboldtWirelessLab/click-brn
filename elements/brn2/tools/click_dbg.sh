#!/bin/bash

if [ "x$DEBUG" != "x" ]; then
  DELDEBUG=$DEBUG
#  DELDEBUG=$(( $DEBUG + 1))
#  echo $DELDEBUG
  if [ $DELDEBUG -le 5 ] && [ $DELDEBUG -gt 0 ]; then
    cat $1 | sed -e "s#//[1-$DELDEBUG]/##g" -e "s#/\*[1-$DELDEBUG]/##g" -e "s#/[1-$DELDEBUG]\*/##g" > $2
  else
    cp $1 $2
  fi
else
  cp $1 $2
#  echo "huu"
fi
