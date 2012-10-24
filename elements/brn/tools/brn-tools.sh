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

if [ "x$CLEAN" = "x" ]; then
  CLEAN=1
fi

if [ "x$CPUS" = "x" ]; then
  if [ -f /proc/cpuinfo ]; then
    CPUS=`grep -e "^processor" /proc/cpuinfo | wc -l`
  else
    CPUS=1
  fi
fi

echo "Use $CPUS cpus"

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
  (cd click-brn;touch ./configure; /bin/sh brn-conf.sh tools; XCFLAGS="-fpermissive -fPIC" /bin/sh brn-conf.sh ns2_userlevel; make -j $CPUS) 2>&1 | tee click_build.log
fi

(cd brn-ns2-click; CLEAN=$CLEAN DEVELOP=$DEVELOP VERSION=5 PREFIX=$DIR/ns2 CPUS=$CPUS CLICKPATH=$CLICKPATH ./install_ns2.sh) 2>&1 | tee ns2_build.log

#if [ "x$BUILDCLICKSCRIPTS" = "xyes" ]; then
#  (cd click-brn-scripts; ./build.sh)
#fi

if [ $CLEAN -eq 1 ]; then
  rm -rf $DIR/brn-ns2-click
fi

echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:$DIR/click-brn/ns/:$DIR/ns2/lib" > $DIR/brn-tools.bashrc
echo "export PATH=$DIR/ns2/bin/:$CLICKPATH/userlevel/:$CLICKPATH/tools/click-align/:$DIR/helper/simulation/bin/:$DIR/helper/evaluation/bin:\$PATH" >> $DIR/brn-tools.bashrc

cat $FULLFILENAME | grep "^#INFO" | sed -e "s/#INFO[[:space:]]*//g" -e "s#TARGETDIR#$DIR#g"

if [ "x$DISABLE_TEST" = "x1" ]; then
  echo "Test disabled"
  rm -f click_build.log ns2_build.log
else
  echo "Start Tests"

  . $DIR/brn-tools.bashrc

  (cd $DIR/click-brn-scripts/; sh ./test.sh) > test.log

  #less test.log

  TESTS_OVERALL=`cat test.log | wc -l`
  TESTS_OK=`cat test.log | awk '{print $3}' | grep "ok" | wc -l`

  echo "$TESTS_OK of $TESTS_OVERALL tests finished without errors. See $DIR/click-brn-scripts/testbed.pdf for more details."

  if [ $TESTS_OK -ne $TESTS_OVERALL ]; then
    echo "Detect failures. Please send test.log, click_build.log and ns2_build.log (hwl-team)."
  else
    rm -f test.log click_build.log ns2_build.log
  fi

fi

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
