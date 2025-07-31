
-include ../../.common_settings

# Avoid influences from user's shell startup file
ENV=

# Name of makefile
MAKEFILE=Makefile

# Base for target name
NAME=src

# destination base path for libraries and programs
DEST=../

GENERATED_HEADERS=

# $(SRCS) is the list of source files used.
SRCS=$(shell ls *.c *.C *.cpp *.l 2>/dev/null)

# List of object files to be created
OBJS_L=$(SRCS:.l=.o)
OBJS_Y=$(OBJS_L:.y=.o)
OBJS_C=$(OBJS_Y:.C=.o)
OBJS_CPP=$(OBJS_C:.cpp=.o)
OBJS=$(sort $(OBJS_CPP:.c=.o))

# shared libraries to be linked to target
SH_LIBS= -ldriver_dev_kits

# Compilers to be used.
CC=/usr/bin/gcc -m64
CXX=/usr/bin/g++ -m64

GUI_PROCESS_PATH1=$(PHCONTROL_ROOT)/drivers/Generic_93K_Driver/tools/bin/guiProcess
GUI_PROCESS_PATH2=/opt/hp93000/testcell/phcontrol/drivers/Generic_93K_Driver/tools/bin/guiProcess

# Compile flags to be used
CFLAGS= -fPIC -DGUIPATH1=\"$(GUI_PROCESS_PATH1)\" -DGUIPATH2=\"$(GUI_PROCESS_PATH2)\"
CFLAGS_DEBUG=-g -fPIC -DGUIPATH1=\"$(GUI_PROCESS_PATH1)\" -DGUIPATH2=\"$(GUI_PROCESS_PATH2)\"
CXXFLAGS= -fPIC -DGUIPATH1=\"$(GUI_PROCESS_PATH1)\" -DGUIPATH2=\"$(GUI_PROCESS_PATH2)\"
CXXFLAGS_DEBUG=-g -fPIC -DGUIPATH1=\"$(GUI_PROCESS_PATH1)\" -DGUIPATH2=\"$(GUI_PROCESS_PATH2)\"

LD=$(CXX)
LDFLAGS=

# include path
INCLUDES = -I../../include/ -I../gui -I$(PHCONTROL_ROOT)/include

YACC=/usr/bin/bison
YFLAGS=-d

LEX=/usr/bin/flex
LFLAGS=

# Files to remove in make clean or make clobber
CLEAN_FILES=$(OBJS) core *~

# Overriding LD for building object libraries
LD=/usr/bin/ld
LDFLAGS=-fatal-warnings -melf_x86_64

CLOBBER_FILES=$(OBJECT_LIBRARY) 

# Rules to create object library
OBJECT_LIBRARY=$(DEST)src.o

all: $(OBJECT_LIBRARY)

$(OBJECT_LIBRARY): $(OBJS)  
	rm -f $(OBJECT_LIBRARY)
	$(LD) -r $(LDFLAGS) $(OBJS) \
	-o $(OBJECT_LIBRARY)

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
	$(CXX) $(CFLAGS) $(INCLUDES) $(CFLAGS) $(SRCS)  -c 

