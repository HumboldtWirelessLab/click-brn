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


FULLFILENAME=`basename $0`
FULLFILENAME=$DIR/$FULLFILENAME

GITHOST=localhost

git clone ssh://$GITHOST/home/sombrutz/repository/students/click-brn/.git
git clone ssh://$GITHOST/home/sombrutz/repository/brn-ns2-click.git
git clone ssh://$GITHOST/home/sombrutz/repository/click-brn-scripts.git

(cd click-brn;touch ./configure; /bin/sh brn-conf.sh ns2_userlevel; make)
(cd brn-ns2-click; VERSION=5 PREFIX=$DIR/ns2 CLICKPATH=$DIR/click-brn ./install_ns2.sh)
(cd click-brn-scripts; ./build.sh)

rm -rf $DIR/brn-ns2-click

echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:$DIR/click-brn/ns/:$DIR/ns2/lib" > $DIR/brn-tools.bashrc
echo "export PATH=$DIR/ns2/bin/:\$PATH" >> $DIR/brn-tools.bashrc

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
