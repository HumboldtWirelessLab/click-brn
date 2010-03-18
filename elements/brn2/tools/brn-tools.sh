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

GITHOST=localhost

git clone ssh://$GITHOST/home/sombrutz/repository/students/click-brn/.git
git clone ssh://$GITHOST/home/sombrutz/repository/brn-ns2-click.git
git clone ssh://$GITHOST/home/sombrutz/repository/click-brn-scripts.git

(cd click-brn;touch ./configure; /bin/sh brn-conf.sh ns2_userlevel; make)
(cd brn-ns2-click; VERSION=4 PREFIX=$DIR/ns2 CLICKPATH=$DIR/click-brn ./install_ns2.sh)

rm -rf $DIR/brn-ns2-click

echo "export LD_LIBRARY_PATH=\$LD_LIBRARY_PATH:$DIR/click-brn/ns/:$DIR/ns2/lib" > $DIR/brn-tools.bashrc
echo "export PATH=$DIR/ns2/bin/:$PATH" >> $DIR/brn-tools.bashrc

echo "Use \"source $DIR/brn-tools.bashrc\" to setup the path-var or add stuff to .bashrc"