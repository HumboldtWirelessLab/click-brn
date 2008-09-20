#! /bin/sh

CONFOPTION="--enable-wifi --enable-brnnew --enable-dhcp --enable-analysis --enable-tools=host"

for op in $@; do
  
    case "$op" in
	"kernel")
	    CONFOPTION="$CONFOPTION --enable-linuxmodule --with-linux=/usr/src/linux-2.6.19.2-click-1.6.0 CFLAGS=\"-g\""
	    ;;
	"jist")
	    ;;
	"ns2")
	     CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-userlevel --enable-nsclick --disable-threads --prefix=`pwd`/../../local CFLAGS=\"-g -O0\" CXXFLAGS=\"-g -O0\""
	    ;;
	"userlevel")
	    ;;
	    *)
	    echo "Unknown target: $op"
	    ;;
    esac

done

echo "./configure $CONFOPTION"

eval ./configure $CONFOPTION 

exit 0
