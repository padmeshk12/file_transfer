HP83000_ROOT=/opt/hp93000/soc

#-include $(HP83000_ROOT)/prod_pws/lbin/Makefile.verigy.include

# specify the compiler options here 
CFLAGS  = 

# Specify any loader commands and libraries here
# the loader should be the same as the compiler for the main subroutine
CC      = /usr/bin/g++
LD	=$(CC)
INCLUDE = -I/usr/include

ifeq ($(XOC_OS_SUFFIX),)
LDFLAGS = -L/usr/X11R6/lib
else
LDFLAGS = -L/usr/lib
endif
LIBS    = -lX11

# list all the object files here
#       and use `\' as the last character
#       to continue to the next line
OBJS    =       show_win.o

# specify the name of the executable to be created
EXE     = ../bin/show_win

all : $(EXE)

$(EXE) : $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDE) -o $(EXE) $(OBJS) $(LIBS)



clean :
	-rm -f show_win core $(OBJS)
 
clobber : clean
	-rm -f $(EXE)
