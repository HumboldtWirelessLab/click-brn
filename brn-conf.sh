#! /bin/sh

if [ -e ./configure ]; then
  touch ./configure
fi

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

#CONFOPTION="--enable-wifi --enable-brn --enable-dhcp --enable-analysis"
CONFOPTION="--enable-wifi --enable-brn --enable-analysis"
#CONFOPTION="--enable-wifi --enable-analysis"

if [ "x$TARGET" = "xmips" ];then
  CONFOPTION="$CONFOPTION --host=mipsel-linux --enable-tools=host --enable-ialign"
  XCFLAGS="$XCFLAGS -static"
  GCCPREFIX="mipsel-linux-"
else
  if [ "x$TARGET" = "xarm" ]; then
    CONFOPTION="$CONFOPTION --host=arm-linux-uclibcgnueabi --enable-tools=host"
    XCFLAGS="$XCFLAGS -static"
    GCCPREFIX="arm-linux-uclibcgnueabi-"
  else
    if [ "x$TARGET" = "xi386" ]; then
      CONFOPTION="$CONFOPTION --host=i386-linux --enable-tools=host --enable-ialign"
      XCFLAGS="$XCFLAGS -static"
      GCCPREFIX="i386-linux-"
    else
      if [ "x$TARGET" = "xmips2" ]; then
        CONFOPTION="$CONFOPTION --host=mips-linux --enable-tools=host --enable-ialign"
        XCFLAGS="$XCFLAGS -static"
        GCCPREFIX="mips-linux-"
      else
        CONFOPTION="$CONFOPTION --enable-tools=host"
      fi
    fi
  fi
fi

if [ "x$1" = "x" ]; then
  #CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --enable-jistclick --prefix=`pwd`/../../local CFLAGS=\"-g $XCFLAGS\" CXXFLAGS=\" $CXXFLAGS -g $XCFLAGS\""
  CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --prefix=`pwd`/../../local CFLAGS=\"-g $XCFLAGS\" CXXFLAGS=\" $CXXFLAGS -g $XCFLAGS\""

fi

if [ ! "x$WALL" = "x0" ]; then
  XCFLAGS="$XCFLAGS -Wall -Wextra -fpermissive"
fi

for op in $@; do

    case "$op" in
	"kernel")
	    CONFOPTION="$CONFOPTION --enable-linuxmodule --with-linux=$KERNELPATH --with-linux-map=$SYSTEMMAP --enable-fixincludes --disable-userlevel"
	    ;;
	"patched_kernel")
	    CONFOPTION="$CONFOPTION --enable-linuxmodule --with-linux=$KERNELPATH --with-linux-map=$SYSTEMMAP --disable-userlevel"
	    ;;
	"jist")
	    if [ "x$JAVA_HOME" = "x" ]; then
	      JAVA_HOME=`ant -diagnostics | grep java.home | sed "s#jre##g" | awk '{print $3}'`
	    fi

	    JAVAINCLUDE="$JAVA_HOME/include"

	    CONFOPTION="$CONFOPTION --disable-linuxmodule --disable-userlevel --enable-jistclick --disable-threads --prefix=`pwd`/click_install CFLAGS=\"-g $XCFLAGS -I$JAVAINCLUDE\" CXXFLAGS=\"-g $XCFLAGS -I$JAVAINCLUDE\""
	    ;;
	"ns2")
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-userlevel --enable-nsclick --disable-threads --prefix=`pwd`/click_install CFLAGS=\"-g $XCFLAGS\" CXXFLAGS=\"-g $XCFLAGS\""
	    ;;
	"userlevel")
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-userlevel --disable-threads --prefix=`pwd`/click_install CFLAGS=\"-g $XCFLAGS\" CXXFLAGS=\"-g $XCFLAGS\""
	    ;;
	"ns2_userlevel")
	    CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --prefix=`pwd`/click_install CFLAGS=\"-g $XCFLAGS\" CXXFLAGS=\"-g $XCFLAGS\""
	    ;;
	"sim_userlevel")
	    if [ "x$JAVA_HOME" = "x" ]; then
	      JAVA_HOME=`ant -diagnostics | grep java.home | sed "s#jre##g" | awk '{print $3}'`
	    fi

	    JAVAINCLUDE="$JAVA_HOME/include"

	    if [ ! -e $JAVAINCLUDE ]; then
		CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --prefix=`pwd`/click_install CFLAGS=\"-g $XCFLAGS\" CXXFLAGS=\"-g $XCFLAGS\""
	    else
		CONFOPTION="$CONFOPTION --disable-linuxmodule --enable-dmalloc --disable-threads --enable-userlevel --enable-nsclick --enable-jistclick --prefix=`pwd`/click_install CFLAGS=\"-g $XCFLAGS -I$JAVAINCLUDE\" CXXFLAGS=\"-g $XCFLAGS -I$JAVAINCLUDE\""
	    fi
	    ;;
	"tools")
	    #( cd elements/brn/tools/tinyxml; make clean; rm -f *.o; CROSS_COMPILE="$GCCPREFIX" CC="$GCCPREFIX\gcc" CXX="$GCCPREFIX\g++" LD="$GCCPREFIX\g++" make libtinyxml.a ; rm -f *.o;CROSS_COMPILE="$GCCPREFIX" CC="$GCCPREFIX\gcc" CXX="$GCCPREFIX\g++" LD="$GCCPREFIX\g++" EXTRA_CXXFLAGS="-fPIC" make libtinyxml.so; make install )
	    exit 0
	    ;;
	    *)
	    echo "Unknown target: $op"
	    exit 0
	    ;;
    esac

done

echo "./configure $CONFOPTION"

if [ "x$TARGET" = "xmips" ] || [ "x$TARGET" = "xarm" ]; then
  eval ARCH=$TARGET ./configure $CONFOPTION
else
  eval ./configure $CONFOPTION
fi

exit 0

#########################################
# NOTICE for git: git reset --hard HEAD #
#########################################
