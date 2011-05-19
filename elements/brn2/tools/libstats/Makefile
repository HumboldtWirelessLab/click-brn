DEBUG          := NO

PROFILE        := NO

CC     := ${CROSS_COMPILE}gcc
CXX    := ${CROSS_COMPILE}g++
LD     := ${CROSS_COMPILE}g++
AR     := ${CROSS_COMPILE}ar rc
RANLIB := ${CROSS_COMPILE}ranlib

DEBUG_CFLAGS     := -Wall -Wno-format -g -DDEBUG
RELEASE_CFLAGS   := -Wall -Wno-unknown-pragmas -Wno-format -O3

LIBS		 :=

DEBUG_CXXFLAGS   := ${DEBUG_CFLAGS} 
RELEASE_CXXFLAGS := ${RELEASE_CFLAGS}

DEBUG_LDFLAGS    := -g
RELEASE_LDFLAGS  :=

ifeq (YES, ${DEBUG})
   CFLAGS       := ${DEBUG_CFLAGS}
   CXXFLAGS     := ${DEBUG_CXXFLAGS}
   LDFLAGS      := ${DEBUG_LDFLAGS}
else
   CFLAGS       := ${RELEASE_CFLAGS}
   CXXFLAGS     := ${RELEASE_CXXFLAGS}
   LDFLAGS      := ${RELEASE_LDFLAGS}
endif

ifeq (YES, ${PROFILE})
   CFLAGS   := ${CFLAGS} -pg -O3
   CXXFLAGS := ${CXXFLAGS} -pg -O3
   LDFLAGS  := ${LDFLAGS} -pg
endif

CXXFLAGS := ${CXXFLAGS} ${EXTRA_CXXFLAGS}

#****************************************************************************
# Include paths
#****************************************************************************
INCS :=

#****************************************************************************
# Makefile code common to all platforms
#****************************************************************************

CFLAGS   := ${CFLAGS}   ${DEFS}
CXXFLAGS := ${CXXFLAGS} ${DEFS}

#****************************************************************************
# Targets of the build
#****************************************************************************

OUTPUT := libstats.a
OUTPUT2 := libstats.so

all: ${OUTPUT} ${OUTPUT2}


#****************************************************************************
# Source files
#****************************************************************************

SRCS := stats.c

# Add on the sources for libraries
SRCS := ${SRCS}

OBJS := $(addsuffix .o,$(basename ${SRCS}))

#****************************************************************************
# Output
#****************************************************************************

${OUTPUT}: ${OBJS}
	$(AR) $@ $?

${OUTPUT2}: ${OBJS}
	$(CC) $(CFLAGS) -shared -o $@ -Wl,-soname,$@ $?

#****************************************************************************
# common rules
#****************************************************************************

# Rules for compiling source files to object files
%.o : %.cpp
	${CXX} -c ${CXXFLAGS} ${INCS} $< -o $@

%.o : %.c
	${CC} -c ${CFLAGS} ${INCS} $< -o $@

clean:
	-rm -f core ${OBJS} ${OUTPUT} ${OUTPUT2}

depend:
	#makedepend ${INCS} ${SRCS}

install: uninstall
	ln -s ../elements/brn2/tools/libstats/libstats.a ../../../../lib/libstats.a
	ln -s ../elements/brn2/tools/libstats/libstats.so ../../../../lib/libstats.so
	ln -s ../elements/brn2/tools/libstats/libstats.a ../../../../userlevel/libstats.a
	ln -s ../elements/brn2/tools/libstats/libstats.so ../../../../userlevel/libstats.so
#	ln -s ../elements/brn2/tools/libstats/libstats.a ../../../../ns/libstats.a
	ln -s ../elements/brn2/tools/libstats/libstats.so ../../../../ns/libstats.so
#	ln -s ../elements/brn2/tools/libstats/libstats.a ../../../../jist/libstats.a
	ln -s ../elements/brn2/tools/libstats/libstats.so ../../../../jist/libstats.so
	ln -s ../elements/brn2/tools/libstats ../../../../include/libstats

uninstall:
	rm -f ../../../../lib/libstats.* ../../../../userlevel/libstats.* ../../../../ns/libstats.* ../../../../jist/libstats.* ../../../../include/libstats

stats.o: stats.h stats.h
