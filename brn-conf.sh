#! /bin/sh
#TODO: add support for mips

#CONFOPTION="--enable-wifi --enable-brn --enable-brn2 --enable-dhcp --enable-analysis"
CONFOPTION="--enable-wifi --enable-brn2 --enable-dhcp --enable-analysis"

if [ "x$TARGET" = "xmips" ];then
  CONFOPTION="$CONFOPTION --host=mipsel-linux --enable-tools=host"
else
  CONFOPTION="$CONFOPTION --enable-tools=host"
fi

if [ "x$1" = "x" ]; then
  CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --enable-jistclick --prefix=`pwd`/../../local CFLAGS=\"-g\" CXXFLAGS=\" -g\""
fi

for op in $@; do
  
    case "$op" in
	"kernel")
	    CONFOPTION="$CONFOPTION --enable-linuxmodule --with-linux=/usr/src/linux-2.6.19.2-click-1.6.0 CFLAGS=\"-g\""
	    ;;
	"jist")
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --disable-userlevel --enable-jistclick --disable-threads --prefix=`pwd`/../../local CFLAGS=\"-g\" CXXFLAGS=\"-g $CXXFLAGS\""
	    ;;
	"ns2")
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-userlevel --enable-nsclick --disable-threads --prefix=`pwd`/../../local CFLAGS=\"-g -O0\" CXXFLAGS=\"-g -O0\""
	    ;;
	"userlevel")
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-userlevel --disable-threads --prefix=`pwd`/../../local CFLAGS=\"-g $XCFLAGS\" CXXFLAGS=\" -g $XCFLAGS\""
	    ;;
	"ns2_userlevel")
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --prefix=`pwd`/../../local CFLAGS=\"-g\" CXXFLAGS=\" -g\""
	    ;;
	"sim_userlevel")
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --enable-jistclick --prefix=`pwd`/../../local CFLAGS=\"-g\" CXXFLAGS=\" -g\""
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