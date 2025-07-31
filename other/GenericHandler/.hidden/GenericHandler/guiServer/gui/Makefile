
-include ../../.common_settings

# Avoid influences from user's shell startup file
ENV=

# Name of makefile
MAKEFILE=Makefile

# Base for target name
NAME=guiProcess

# destination base path for libraries and programs
DEST=..

GENERATED_HEADERS=phGui_.c-parser.h
GENERATED_SRCS= phGui_.c-scanner.c phGui_.c-parser.c

# $(SRCS) is the list of source files used.
SRCS= $(shell ls *.cpp *.C *.c 2>/dev/null)

# List of object files to be created
OBJS_L=$(SRCS:.l=.o)
OBJS_Y=$(OBJS_L:.y=.o)
OBJS_C=$(OBJS_Y:.C=.o)
OBJS_CPP=$(OBJS_C:.cpp=.o)
OBJS=$(sort $(OBJS_CPP:.c=.o))

# system libraries to be linked to target, like -lm -lX11 etc.
SYSLIBS=-lXm -lXt -lX11

# External object files required to build target
EXTERNAL_OBJS= ../../tools/src.o
# Compilers to be used.  CC=/usr/bin/gcc -m64
CXX=/usr/bin/g++ -m64

# Compile flags to be used
CFLAGS= -fPIC
CFLAGS_DEBUG=-g -fPIC
CXXFLAGS= -fPIC
CXXFLAGS_DEBUG=-g -fPIC

LD=$(CC)
LDFLAGS= -Wl,-fatal-warnings -Wl,-melf_x86_64

# Include Paths
INCLUDES= -I../../include -I../src

YACC=/usr/bin/bison
YFLAGS=-d

LEX=/usr/bin/flex
LFLAGS=

# Files to remove in make clean or make clobber
CLEAN_FILES=$(OBJS) $(GENERATED_HEADERS) $(GENERATED_SRCS) core *~
CLOBBER_FILES=$(PROGRAM) 

# Rules to create an executable
PROGRAM=$(DEST)/bin/$(NAME)

all:	$(PROGRAM)
$(PROGRAM):     phGui_.c-scanner.c phGui_.c-parser.c phGui_.c-parser.h $(OBJS) $(EXTERNAL_OBJS) 
		rm -f $(PROGRAM)
		$(CXX) -Wl,-z,now $(LDFLAGS) $(OBJS) $(EXTERNAL_OBJS) $(SYSLIBS) \
		-o $(PROGRAM)

debug:
	$(MAKE) -f $(MAKEFILE) CFLAGS='$(CFLAGS_DEBUG)' CXXFLAGS='$(CXXFLAGS_DEBUG)' all

clean:
	rm -f $(CLEAN_FILES)

clobber:
	rm -f $(CLEAN_FILES) $(CLOBBER_FILES)

depend:

install:
	@echo "nothing to be installed"

$(OBJS): $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDES) $(SRCS) -c

phGui_.c-scanner.o: phGui_.c-scanner.c

phGui_.c-scanner.c:	phGui_.c-scanner.l 
	$(LEX) $(LFLAGS) -PphGui_ -ophGui_.c-scanner.c phGui_.c-scanner.l
	@: LEX_VERSION="/usr/bin/flex version 2.5.37" OS_NAME="linux" OS_VERSION="RHEL7" 

%.c-parser.c %.c-parser.h:	%.c-parser.y 
	$(YACC) -by $(YFLAGS) -p$* $<
	mv y.tab.c $*.c-parser.c
	mv y.tab.h $*.c-parser.h
	@: YACC_VERSION="bison (GNU Bison) 2.7" OS_NAME="linux" OS_VERSION="RHEL7" 

%.cpp-parser.C %.cpp-parser.h:	%.cpp-parser.y 
	$(YACC) -by $(YFLAGS) -p$* $<
	mv y.tab.c $*.cpp-parser.C
	mv y.tab.h $*.cpp-parser.h
	@: YACC_VERSION="bison (GNU Bison) 2.7" OS_NAME="linux" OS_VERSION="RHEL7" 

phGui_.c-parser.o: phGui_.c-parser.c
