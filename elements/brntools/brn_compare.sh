#!/bin/sh

NOTFOUND=""

for i in `find $1 -type f`; do
 
  FILENAME=`basename $i`
  
  NEWNAME=`find $2 -type f -name $FILENAME | head -n 1`

  if [ "x$NEWNAME" != "x" ]; then
    #echo "$i -> $NEWNAME"
    diff $i $NEWNAME
  else
  	NOTFOUND="$NOTFOUND $FILENAME"
  fi
done

echo "NOT FOUND"
echo "$NOTFOUND"

exit 0
