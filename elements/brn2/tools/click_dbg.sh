#!/bin/bash

if [ "x$DEBUG" = "x" ]; then
  DEBUG=2
else
  if [ $DEBUG -gt 4 ] || [ $DEBUG -lt 0 ]; then
    DEBUG=2
  fi
fi

#DELDEBUG=$(( $DEBUG + 1))

cat $1 | sed -e "s#//[0-$DEBUG]/##g" -e "s#/\*[0-$DEBUG]/##g" -e "s#/[0-$DEBUG]\*/##g" -e "s#DEBUGLEVEL#$DEBUG#g" > $2
