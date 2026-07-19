# makefile

PROGRAM=tgui.a
CC=g++
AR=ar

EFLAGS= -DRUN_ENVIRONMENT_LINUX
SFLAGS= -DCHINESE_SUPPORT -DMOUSE_SUPPORT -DRESMGT_SUPPORT
DFLAGS= -DMODE_DEBUG
CFLAGS= -O2 -lpthread

VPATH= .
OBJECTS:=$(patsubst %.cpp,%.o,$(wildcard ${VPATH}/*.cpp))

all:${PROGRAM}
${PROGRAM}:${OBJECTS}
	${AR} -cr  ${PROGRAM} ${notdir ${OBJECTS}}

%.o:%.cpp
	${CC} ${EFLAGS} ${SFLAGS} ${DFLAGS} ${CFLAGS} -c $<

clean:
	rm $(notdir ${OBJECTS}) -f
	rm ${PROGRAM} -f
	rm *~ -f

# END