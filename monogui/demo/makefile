# makefile

APP=demo
CC=g++

EFLAGS=
# -DRUN_ENVIRONMENT_LINUX
SFLAGS=
# -DCHINESE_SUPPORT -DMOUSE_SUPPORT -DRESMGT_SUPPORT
DFLAGS= -DMODE_DEBUG
LFLAGS= -I/usr/include/X11 -L/usr/X11R6/lib -pthread
CFLAGS= -O2
FLAGS = $(EFLAGS) $(SFLAGS) $(DFLAGS) $(LFLAGS) $(CFLAGS)
XFLAGS= -lX11

VPATH= .
SRCS = $(wildcard $(VPATH)/*.cpp)
OBJS = $(patsubst %.cpp, %.o, $(SRCS))
LIBS = ../tgui/tgui.a

$(APP): $(OBJS) $(LIBS)
	$(CC) -o $(APP) $(FLAGS) $(XFLAGS) $(OBJS) $(LIBS)

%.o: %.cpp
	$(CC) $(FLAGS) -c $<

clean:
	rm $(notdir $(OBJS)) -f
	rm $(APP) -f
	rm *~ -f

# END