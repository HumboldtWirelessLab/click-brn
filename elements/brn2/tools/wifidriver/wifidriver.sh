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

if [ "x$GITHOST" = "x" ]; then
  GITHOST=nfs-student
fi


if [ "x$GITUSER" = "x" ]; then
  GITUSER=gituser
fi

case "$1" in
    "clone")
        if [ ! -e brn-compat ]; then
          git clone ssh://$GITUSER@$GITHOST/home/sombrutz/repository/brn-compat/.git
	fi
        if [ ! -e brn-compat-wireless-2.6 ]; then
	  git clone ssh://$GITUSER@$GITHOST/home/sombrutz/repository/brn-compat-wireless-2.6/.git
	fi
        if [ ! -e brn-linux-next ]; then
	  git clone ssh://$GITUSER@$GITHOST/home/sombrutz/repository/brn-linux-next/.git
	fi
        ;;
    "build")
        export GIT_COMPAT_TREE=$DIR/brn-compat
        export GIT_TREE=$DIR/brn-linux-next
        (export GIT_COMPAT_TREE=$DIR/brn-compat; export GIT_TREE=$DIR/brn-linux-next; cd brn-compat-wireless-2.6/; ./scripts/admin-refresh.sh)
        (export GIT_COMPAT_TREE=$DIR/brn-compat; export GIT_TREE=$DIR/brn-linux-next; cd brn-compat-wireless-2.6/; ./scripts/driver-select ath)
	if [ "x$NOCROSS" = "x1" ]; then
          (export GIT_COMPAT_TREE=$DIR/brn-compat; export GIT_TREE=$DIR/brn-linux-next; cd brn-compat-wireless-2.6/; make)
	else 
          (export GIT_COMPAT_TREE=$DIR/brn-compat; export GIT_TREE=$DIR/brn-linux-next; cd brn-compat-wireless-2.6/; sh ./make_mips.sh)
	fi
        ;;
    "pull")
        (cd brn-compat-wireless-2.6/; git pull)
        (cd brn-compat/; git pull)
	(cd brn-linux-next/; git pull)
        ;;
    "copy")
	(cd brn-compat-wireless-2.6/;find . -name "*.ko" -print0 | xargs -0 cp --target=/testbedhome/testbed/helper/nodes/lib/modules/mips-wndr3700/2.6.32.27/)
	;;
    "clean")
        export GIT_COMPAT_TREE=$DIR/brn-compat
        export GIT_TREE=$DIR/brn-linux-next
        (cd brn-compat-wireless-2.6/; ./scripts/admin-clean.sh)
        ;;
     "*")
        echo "Use $0 build!"
        ;;
esac


exit 0
