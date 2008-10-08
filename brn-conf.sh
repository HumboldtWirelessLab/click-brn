#! /bin/sh
#TODO: add support for mips

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
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-userlevel --disable-threads --prefix=`pwd`/../../local CFLAGS=\"-g\" CXXFLAGS=\" -g\""
	    ;;
	"ns2_userlevel")
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --prefix=`pwd`/../../local CFLAGS=\"-g\" CXXFLAGS=\" -g\""
	    ;;
	    *)
	    echo "Unknown target: $op"
	    ;;
    esac

done

echo "./configure $CONFOPTION"

eval ./configure $CONFOPTION 

exit 0


#########################################
# NOTICE for git: git reset --hard HEAD #
#########################################