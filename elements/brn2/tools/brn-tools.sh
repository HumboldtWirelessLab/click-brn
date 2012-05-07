#!/bin/sh

dir=$(dirname "$0")
pwd=$(pwd)

SIGN=`echo $dir | cut -b 1`

case "$SIGN" in
  "/")
        DIR=$dir
        ;;
  ".")
        DIR=$pwd/$dir
        ;;
   *)
        echo "Error while getting directory"
        exit -1
        ;;
esac


echo "Make sure that you have the following packages:"
echo " * g++"
echo " * autoconf"
echo " * libx11-dev"
echo " * libxt-dev"
echo " * libxmu-dev"
echo " * flex"
echo " * bison"
echo ""
echo "Add following lines to .ssh/config"
echo "Host gruenau"
echo "   User username"
echo "   HostName gruenau.informatik.hu-berlin.de"
echo "	 LocalForward 23452 sar.informatik.hu-berlin.de:2222"
echo ""
echo "Host gitsar"
echo "   User username"
echo "   HostName localhost"
echo "   ProxyCommand ssh -q gruenau netcat sar 2222"
#echo "   Port 23452"
echo ""
echo "Open a terminal and login to gruenau using \"ssh gruenau\". Don't close the terminal until you finish the installation." 


if [ "x$1" = "xhelp" ]; then
  exit 0
fi

FULLFILENAME=`basename $0`
FULLFILENAME=$DIR/$FULLFILENAME

GITHOST=gitsar
#GITHOST=nfs-student

if [ "x$DEVELOP" = "x" ]; then
  DEVELOP=1
fi

#***********************************************************************
#*************************** G E T   S O U R C E S *********************
#***********************************************************************

if [ "x$CLICKPATH" = "x" ]; then
  git clone ssh://$GITHOST/home/sombrutz/repository/click-brn/.git
  CLICKPATH=$DIR/click-brn
  BUILDCLICK=yes
else
  BUILDCLICK=no
fi

git clone ssh://$GITHOST/home/sombrutz/repository/brn-ns2-click.git

if [ "x$CLICKSCRIPTS" = "x" ]; then
  git clone ssh://$GITHOST/home/sombrutz/repository/click-brn-scripts.git
  BUILDCLICKSCRIPTS=yes
else
  BUILDCLICKSCRIPTS=no
fi

if [ "x$HELPER" = "x" ]; then
  git clone ssh://$GITHOST/home/sombrutz/repository/helper.git
fi

if [ "x$DEVELOP" = "x1" ]; then
  mkdir -p $DIR/ns2/src
  (cd $DIR/ns2/src; git clone ssh://$GITHOST/home/sombrutz/repository/ns-2.34.git)
fi

#***********************************************************************
#******************************** B U I L D ****************************
#***********************************************************************

if [ "x$BUILDCLICK" = "xyes" ]; then
  (cd click-brn;touch ./configure; /bin/sh brn-conf.sh tools; XCFLAGS="-fpermissive -fPIC" /bin/sh brn-conf.sh ns2_userlevel; make)
fi

(cd brn-ns2-click; DEVELOP=$DEVELOP VERSION=5 PREFIX=$DIR/ns2 CLICKPATH=$CLICKPATH ./install_ns2.sh)

#if [ "x$BUILDCLICKSCRIPTS" = "xyes" ]; then
#  (cd click-brn-scripts; ./build.sh)
#fi

rm -rf $DIR/brn-ns2-click

echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:$DIR/click-brn/ns/:$DIR/ns2/lib" > $DIR/brn-tools.bashrc
echo "export PATH=$DIR/ns2/bin/:$CLICKPATH/userlevel/:$CLICKPATH/tools/click-align/:$DIR/helper/simulation/bin/:$DIR/helper/evaluation/bin:\$PATH" >> $DIR/brn-tools.bashrc

cat $FULLFILENAME | grep "^#INFO" | sed -e "s/#INFO[[:space:]]*//g" -e "s#TARGETDIR#$DIR#g"

exit 0

#INFO
#INFO
#INFO --------------- FINISH ------------------
#INFO
#INFO
#INFO
#INFO
#INFO Well done !
#INFO
#INFO Use "source TARGETDIR/brn-tools.bashrc" to setup the path-var or add stuff to .bashrc
#INFO
#INFO

#HELP Update NS2: CLICKPATH=/XXX/click-brn CLICKSCRIPTS=/XXX/click-brn-scripts/ sh ./brn-tools.sh
