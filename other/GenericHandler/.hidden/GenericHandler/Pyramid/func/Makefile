
-include ../../.common_settings

# Avoid influences from user's shell startup file
ENV=

# Name of makefile MAKEFILE=Makefile
MAKEFILE=Makefile

# Base for target name
NAME=libfunc

# destination base path for libraries and programs
DEST=..

GENERATED_HEADERS=identity.h

# $(SRCS) is the list of source files used.
SRCS=$(shell ls *.c *.cpp *.C 2>/dev/null)

# List of object files to be created
OBJS_L=$(SRCS:.l=.o)
OBJS_Y=$(OBJS_L:.y=.o)
OBJS_C=$(OBJS_Y:.C=.o)
OBJS_CPP=$(OBJS_C:.cpp=.o)
OBJS=$(sort $(OBJS_CPP:.c=.o))

# shared libraries to be linked to target
SH_LIBS= -ldriver_dev_kits -lsecstwo -liconv -lxml2 -lpthread -L $(PHCONTROL_ROOT)/lib -Wl,-rpath=$(PHCONTROL_ROOT)/lib -g

# system libraries to be linked to target, like -lm -lX11 etc.
SYSLIBS=-lm

# External object files required to build target
EXTERNAL_OBJS=

# Compilers to be used.
CC=/usr/bin/gcc -m64
CXX=/usr/bin/g++ -m64

# Compile flags to be used
CFLAGS=-fPIC
CFLAGS_DEBUG=-g -fPIC
CXXFLAGS= -fPIC
CXXFLAGS_DEBUG=-g -fPIC

LD=$(CXX)
LDFLAGS= -Wl,-fatal-warnings -Wl,-melf_x86_64  


# Include path
INCLUDES= -I../../include -I$(PHCONTROL_ROOT)/include -I /usr/include/libxml2/ -std=c++11 -g 

YACC=/usr/bin/bison
YFLAGS=-d

LEX=/usr/bin/flex
LFLAGS=

# Files to remove in make clean or make clobber
CLEAN_FILES=$(OBJS) $(GENERATED_HEADERS) core *~
CLOBBER_FILES=$(SHARED_LIBRARY) makefile.Template

SHARED_LIBRARY=$(DEST)/lib/$(NAME).so

all:	$(SHARED_LIBRARY)

# Build shared library
$(SHARED_LIBRARY): identity.h $(OBJS)
	$(LD) -shared -Wl,-Bdynamic $(LDFLAGS) $(SH_LIBS) $(OBJS) $(SYSLIBS) \
	      -o $(SHARED_LIBRARY)

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
	$(CXX) $(INCLUDES) $(CFLAGS) $(SRCS) -c

# define the identity of the driver plugin, e.g
IDENTITY      = $(shell pwd|xargs dirname|xargs basename) Handler Driver Plugin

identity:;	rm -f identity.h ; \
		echo "#define PHFUNC_IDENT \"$(IDENTITY)\"" >> identity.h ; \
		echo "#define PHFUNC_DATE \""`date`"\"" >> identity.h ; \
		echo "#define PHFUNC_REVISION \"$(REVISION)\"" >> identity.h ; \
		echo "#undef PHFUNC_LOCAL" >> identity.h ; \

identity.h:	identity
