# Name of makefile
MAKEFILE=Makefile

# Base for target name
NAME=libgeneric_93k_driver

# destination base path for libraries and programs
DEST=..

# List of object files to be created
OBJS_L=$(SRCS:.l=.o)
OBJS_Y=$(OBJS_L:.y=.o)
OBJS_C=$(OBJS_Y:.C=.o)
OBJS_CPP=$(OBJS_C:.cpp=.o)
OBJS=$(sort $(OBJS_CPP:.c=.o))

LIBS=
SH_LIBS= -lhw_cor_gio -ldriver_dev_kits
SYSLIBS=-ldl
EXTERNAL_OBJS=../state/src.o ../tcom/src.o ../estate/src.o ../config/src.o ../mhcom/src.o ../event/src.o ../guiServer/src.o ../tools/src.o ../log/src.o ../diagnostics/src.o ../frame/src.o
CC=/usr/bin/gcc -m64
CXX=/usr/bin/g++ -m64
CFLAGS=-fPIC
CFLAGS_DEBUG=-g -fPIC
CXXFLAGS=-fPIC
CXXFLAGS_DEBUG=-g -fPIC
LD=$(CXX)
LDFLAGS=-Wl,-fatal-warnings -Wl,-melf_x86_64
POTENTIAL_GCOV_LDFLAGS_NEEDS_TOBE_AT_THE_END=

YACC=/usr/bin/bison
YFLAGS=-d

LEX=/usr/bin/flex
LFLAGS=

SHELL=/bin/ksh

# Files to remove in make clean or make clobber
CLEAN_FILES=$(OBJS) core *~
CLOBBER_FILES=$(SHARED_LIBRARY) 

SHARED_LIBRARY=$(DEST)/./$(NAME).so

all:	$(SHARED_LIBRARY)

# Build shared library
$(SHARED_LIBRARY):     $(OBJS) $(EXTERNAL_OBJS) $(LIBS)  
	$(LD) -shared -Wl,-Bdynamic $(CFLAGS) $(LDFLAGS) $(SH_LIBS) \
	$(OBJS) $(EXTERNAL_OBJS) $(LIBS) $(SYSLIBS) \
	-o $(SHARED_LIBRARY)

debug:
	$(MAKE) -f $(MAKEFILE) CFLAGS='$(CFLAGS_DEBUG)' CXXFLAGS='$(CXXFLAGS_DEBUG)' all

depend: 

clean:
	rm -f $(CLEAN_FILES)

clobber:
	rm -f $(CLEAN_FILES) $(CLOBBER_FILES)

# install nothing (target used for compatibility only)
install:
	@echo "nothing to be installed"
