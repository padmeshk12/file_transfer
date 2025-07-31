
-include ../../.common_settings

# Avoid influences from user's shell startup file
ENV=

# Name of makefile
MAKEFILE=Makefile

# Base for target name
NAME=src

# destination base path for libraries and programs
DEST=../

GENERATED_HEADERS=phConfAttrYy.c-parser.h phConfConfYy.c-parser.h
GENERATED_SRCS=config.c-scanner.c phConfAttrYy.c-parser.c phConfConfYy.c-parser.c

# $(SRCS) is the list of source files used.
SRCS=$(shell ls *.c *.cpp *.C 2>/dev/null)

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

# Compile flags to be used
CFLAGS= -fPIC
CFLAGS_DEBUG=-g -fPIC
CXXFLAGS= -fPIC
CXXFLAGS_DEBUG=-g -fPIC

LD=$(CXX)
LDFLAGS= -Wl,-fatal-warnings -Wl,-melf_x86_64

# Include path
INCLUDES= -I../../include -I$(PHCONTROL_ROOT)/include

YACC=/usr/bin/bison
YFLAGS=-d --no-lines

LEX=/usr/bin/flex
LFLAGS=

# Files to remove in make clean or make clobber
CLEAN_FILES=$(OBJS) core *~

# Overriding LD for building object libraries
LD=/usr/bin/ld

LDFLAGS= -fatal-warnings -melf_x86_64

CLOBBER_FILES=$(OBJECT_LIBRARY) $(GENERATED_HEADERS) $(GENERATED_SRCS) makefile.Template

# Rules to create object library
OBJECT_LIBRARY=$(DEST)src.o

all: $(OBJECT_LIBRARY)

$(OBJECT_LIBRARY): config.c-scanner.c phConfAttrYy.c-parser.c phConfAttrYy.c-parser.h phConfConfYy.c-parser.c phConfConfYy.c-parser.h $(OBJS)  
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
	$(CXX) $(CFLAGS) $(INCLUDES) $(SRCS) -c 

compare_parser_tables: phConfAttrYy.c-parser.h phConfConfYy.c-parser.h
	diff phConfAttrYy.c-parser.h phConfConfYy.c-parser.h | diff - acceptedDiffOldBison || diff phConfAttrYy.c-parser.h phConfConfYy.c-parser.h | diff - acceptedDiff

config.c-scanner.c: compare_parser_tables

config.c-scanner.c:	config.c-scanner.l 
	$(LEX) $(LFLAGS) -Pconfig -oconfig.c-scanner.c config.c-scanner.l
	@: LEX_VERSION="/usr/bin/flex version 2.5.4" OS_NAME="linux" OS_VERSION="RHEL5" 


%.c-parser.c %.c-parser.h:	%.c-parser.y 
	$(YACC) -by $(YFLAGS) -p$* $<
	mv y.tab.c $*.c-parser.c
	mv y.tab.h $*.c-parser.h
	@: YACC_VERSION="bison (GNU Bison) 2.3" OS_NAME="linux" OS_VERSION="RHEL5" 

%.cpp-parser.C %.cpp-parser.h:	%.cpp-parser.y 
	$(YACC) -by $(YFLAGS) -p$* $<
	mv y.tab.c $*.cpp-parser.C
	mv y.tab.h $*.cpp-parser.h
	@: YACC_VERSION="bison (GNU Bison) 2.3" OS_NAME="linux" OS_VERSION="RHEL5" 

phConfAttrYy.c-parser.o: phConfAttrYy.c-parser.c

phConfConfYy.c-parser.o: phConfConfYy.c-parser.c
