#*************************************************************************************
# NOTICE: PLEASE READ CAREFULLY: ADVANTEST PROVIDES THIS SOFTWARE TO YOU ONLY UPON 
# YOUR ACCEPTANCE OF ADVANTEST'S TERMS OF USE. THE SOFTWARE IS PROVIDED "AS IS" 
# WITHOUT WARRANTY OF ANY KIND AND ADVANTEST'S LIABILITY FOR SUCH SOFTWARE LIMITED 
# TO THE FULLEST EXTENT PERMITTED UNDER THE LAW.
#*************************************************************************************

#--------------------------------------------------------------------
# Modified Makefile as to support building 32bit and 64bit versions
# of the 'load_wafer_description' executable.
#--------------------------------------------------------------------
HP83000_ROOT=/opt/hp93000/soc

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# NOTE: The following two assignments have been leveraged directly from TDC Topic 95795
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Get currently active SmarTest architecture information and configure the compile flag ('-m32' or '-m64')
HP83000_ARCH    = $(shell file ${HP83000_ROOT}/prod_com/sh_lib/libcicpi.so 2>&1 | sed 's/.*ELF \(.*\)-bit.*/-m\1/g')

# Get all HVM library dependencies and create the '-L xxxx' and '-Wl,-rpath xxxx' linker options that will statisfy 
# all of the hvm-api library 'libhvm_cpp.so' dependencies.
HVM_LIB_DEP     = $(shell ${HP83000_ROOT}/system/bin/run -Q -x /usr/bin/ldd ${HP83000_ROOT}/system/lib/libhvm_cpp.so | awk '/\/opt/ { print $$3 }' | xargs -n1 dirname | sort | uniq | sed 's/\(.*\)/-L \1 -Wl,-rpath \1/g')
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

CPP = g++
LD  = g++

INCLUDES =  -I /opt/hp93000/soc/prod_com/include -I ${HP83000_ROOT}/system/include

LIB_PATHS = -L ${HP83000_ROOT}/prod_com/sh_lib  -Wl,-rpath ${HP83000_ROOT}/prod_com/sh_lib  ${HVM_LIB_DEP}

CPPFLAGS = ${HP83000_ARCH} -g3 ${INCLUDES} -Wall

LDFLAGS = ${HP83000_ARCH} ${LIB_PATHS} -lhvm_cpp 

SRCS = $(shell ls *.cpp)

OBJS = $(SRCS:.cpp=.o)

ifeq ($(HP83000_ARCH),-m64)
SUFFIX=-64bit
endif

DEST=../bin
PROGRAM = $(DEST)/load_wafer_description$(SUFFIX)

all: $(PROGRAM)

$(PROGRAM): $(OBJS)
	@echo ""
	@echo "  ---------------------------------------------------"
	@echo "  -- Linking program \"$(PROGRAM)\" ..."
	@echo "  ---------------------------------------------------"
	${LD} $(OBJS) $(LDFLAGS) -o $@
	@echo ""

.cpp.o:
	@echo ""
	@echo "  ---------------------------------------------------"
	@echo "  -- Compiling source \"$<\" ..."
	@echo "  ---------------------------------------------------"
	${CPP} $(CPPFLAGS) -c $<

clobber:
	rm -f $(OBJS)
	rm -f core*
	rm -f  $(PROGRAM)

clean:
	@echo ""
	@echo "  ---------------------------------------------------"
	@echo "  -- cleaning program and objects ..."
	@echo "  ---------------------------------------------------"
	rm -f $(OBJS)
	rm -f core*
	rm -f  $(PROGRAM)

info:
	@echo "HP83000_ARCH = $(HP83000_ARCH)"
	@echo "HVM_LIB_DEP = $(HVM_LIB_DEP)"
