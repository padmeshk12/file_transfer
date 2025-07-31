
-include ../../.common_settings

# Avoid influences from user's shell startup file
ENV=

# Name of makefile
MAKEFILE=Makefile

# Base for target name
NAME=vxi11_server

# destination base path for libraries and programs
DEST=../bin/

GENERATED_HEADERS=

# $(SRCS) is the list of source files used.
SRCS=$(shell ls *.c *.cpp *.C 2>/dev/null)

# List of object files to be created
# Replace the suffixes with .o
OBJS_L=$(SRCS:.l=.o)
OBJS_Y=$(OBJS_L:.y=.o)
OBJS_C=$(OBJS_Y:.C=.o)
OBJS_CPP=$(OBJS_C:.cpp=.o)
OBJS=$(sort $(OBJS_CPP:.c=.o))

# shared libraries to be linked to target
SH_LIBS= -lpthread -lrt

# Compilers to be used.
CC=/usr/bin/gcc -m64
CXX=/usr/bin/g++ -m64

# Compile flags to be used
CFLAGS= -fPIC
CFLAGS_DEBUG=-g -fPIC
CXXFLAGS= -fPIC
CXXFLAGS_DEBUG=-g -fPIC

# Include path
INCLUDES=

# Files to remove in make clean or make clobber
CLEAN_FILES=$(OBJS) core *~

# Overriding LD for building object libraries
LD=/usr/bin/ld

LDFLAGS= -fatal-warnings -melf_x86_64

CLOBBER_FILES=$(OBJECT_BIN) 

# Rules to create object library
OBJECT_BIN=$(DEST)vxi11_server

all: $(OBJECT_BIN)

$(OBJECT_BIN): $(OBJS)
	rm -f $(OBJECT_BIN)
	@echo "Linking $(PROGRAM) ..."
	$(CXX) $(OBJS) $(SH_LIBS) -o $(OBJECT_BIN)
	@echo "done"

debug:
	$(MAKE) -f $(MAKEFILE) CFLAGS='$(CFLAGS_DEBUG)' CXXFLAGS='$(CXXFLAGS_DEBUG)' all

clean:
	rm -f $(CLEAN_FILES)

clobber:
	rm -f $(CLEAN_FILES) $(CLOBBER_FILES)

depend: 
	@echo "skip make depend"

install:
	@echo "nothing to be installed"

$(OBJS): $(SRCS)
	$(CXX) $(CFLAGS) $(INCLUDES) $(SRCS) -c
