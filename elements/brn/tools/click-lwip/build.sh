#!/bin/sh

#TODOSymlinks

GIT=/usr/bin/git

if [ "x$1" = "xclean" ]; then
  (cd lib; make clean)

  rm -rf brn-lwip
  rm -rf brn-lwip-contrib

  exit 0
fi

if [ ! -d brn-lwip ]; then
  ${GIT} clone http://git.savannah.gnu.org/cgit/lwip.git brn-lwip
fi

if [ ! -d brn-lwip-contrib ]; then
  ${GIT} clone http://git.savannah.gnu.org/cgit/lwip/lwip-contrib.git brn-lwip-contrib
fi

(cd lib; make)

exit 0
