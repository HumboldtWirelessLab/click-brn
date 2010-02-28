#! /bin/sh

if [ "x$KERNELPATH" = "x" ]; then
  KERNELUNAME=`uname -r`
  KERNEL_MODLIB="/lib/modules/$KERNELUNAME"
  
  if [ "x$SYSSRC" != "x" ]; then
    KERNEL_SOURCES=$SYSSRC
    KERNEL_HEADERS="$KERNEL_SOURCES/include"
  else
    if [ "x$SYSINCLUDE" != "x" ]; then
      KERNEL_HEADERS="$SYSINCLUDE"
      KERNEL_SOURCES="$KERNEL_HEADERS/.."
    else
      if [ -d $KERNEL_MODLIB/source ]; then
        KERNEL_SOURCES="$KERNEL_MODLIB/source"
      else
        KERNEL_SOURCES="$KERNEL_MODLIB/build"
      fi
      KERNEL_HEADERS="$KERNEL_SOURCES/include"
    fi
  fi

  KERNELPATH=$KERNEL_SOURCES
fi

if [ "x$SYSTEMMAP" = "x" ]; then
  if [ -f $KERNELPATH/System.map ]; then
    SYSTEMMAP="$KERNELPATH/System.map"
  else
    KERNELUNAME=`uname -r`
    SYSTEMMAP="/boot/System.map-$KERNELUNAME"
  fi
fi

#CONFOPTION="--enable-wifi --enable-brn --enable-brn2 --enable-dhcp --enable-analysis"
CONFOPTION="--enable-wifi --enable-brn2 --enable-analysis"

if [ "x$TARGET" = "xmips" ];then
  CONFOPTION="$CONFOPTION --host=mipsel-linux --enable-tools=host"
else
  if [ "x$TARGET" = "xarm" ]; then
    CONFOPTION="$CONFOPTION --host=arm-linux-uclibcgnueabi --enable-tools=host"
  else
    CONFOPTION="$CONFOPTION --enable-tools=host"
  fi
fi

if [ "x$1" = "x" ]; then
  CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --enable-jistclick --prefix=`pwd`/../../local CFLAGS=\"-g\" CXXFLAGS=\" -g\""
fi

for op in $@; do
  
    case "$op" in
	"kernel")
	    CONFOPTION="--enable-linuxmodule --with-linux=$KERNELPATH --with-linux-map=$SYSTEMMAP --enable-fixincludes --disable-userlevel"
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
