ifeq "${SHIPMENT_ROOT}" ""
  SHIPMENT_ROOT=/opt/hp93000/soc
endif

HP83000_ROOT=${SHIPMENT_ROOT}

#
# Advantest 93000 Makefile
#
# Avoid influences from user's shell startup file
ENV=

# include operating system dependent settings
-include $(HP83000_ROOT)/prod_pws/lbin/Makefile.verigy.include

# Name of makefile
MAKEFILE=Makefile

# Base for target name
NAME=pg_templ

# destination base path for libraries and programs
DEST=.

#
#
GENERATED_HEADERS=

# $(SRCS) is the list of source files used.
# $(SRCS) is given as argument to makedepend to create the dependency information
SRCS=$(filter-out ~%, $(wildcard *.c *.C *.cpp))

# List of object files to be created
# Usually, OBJS contains one file for each file in SRCS
# Assuming that SRCS contains only .c, .C, .cpp, .l and .y files,
# OBJS can be obtained from SRCS as follows.
# But it is also possible to define OBJS explicitely if you wish
# to have object files in OBJS whose sources are not in SRCS.
# Since SRCS is only used for makedepend, these object files
# would also be built, but makedepend would create no dependency
# rules for them.
OBJS_L=$(SRCS:.l=.o)
OBJS_Y=$(OBJS_L:.y=.o)
OBJS_C=$(OBJS_Y:.C=.o)
OBJS_CPP=$(OBJS_C:.cpp=.o)
OBJS=$(sort $(OBJS_CPP:.c=.o))

# archive libraries to be linked to target
LIBS=

# shared libraries to be linked to target
SH_LIBS=-L $(HP83000_ROOT)/com/sh_lib -L $(HP83000_ROOT)/prod_com/sh_lib -L $(HP83000_ROOT)/pws/sh_lib -Wl,-rpath,:$(V_UNO_DIR)/ure/lib::$(HP83000_ROOT)/prod_com/sh_lib:$(HP83000_ROOT)/com/sh_lib:$(HP83000_ROOT)/pws/sh_lib -lcicpi -lmc_macc -ldrl -ltstfunc

# system libraries to be linked to target, like -lm -lX11 etc.
SYSLIBS=-lm -lX11

# External object files required to build target
EXTERNAL_OBJS=

# dependency directories to build libraries and objects
ifeq ($(HP83000_ROOT),/opt/hp93000/soc)
        BUILD_DIRS=
else
        BUILD_DIRS= 
endif
# 
# Compilers to be used.
# If the scripts CC_hp83000 and CCP_hp83000 are used, they may
# have options to enable support of purify, purecover, insure or dmalloc.
# These options are specified as purify=[yes|no|<debug tool config file>].
# The default configuration files for the debug tools are located in $(HP83000_ROOT)/com/lbin
CC=/usr/bin/gcc $(V_COMPILE_FLAG)
CXX=/usr/bin/g++ $(V_COMPILE_FLAG)

# Compile flags to be used
CFLAGS= -I $(HP83000_ROOT)/pws/lib -I$(HP83000_ROOT)/prod_com/include -I$(HP83000_ROOT)/com/include -I$(HP83000_ROOT)/mix_sgnl/include -I $(V_UNO_DIR)/sdk/include/ -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -O -D_EXTENDED_ANSI -DHP83000_ROOT=\"/opt/hp93000/soc\" -DFORMATTER_ROOT=\"${SHIPMENT_ROOT}/segments/formatter\" -DHP83000_JAVA_ROOT=\"$(V_JAVA_DIR)\" -DOS_KERNEL=2 -DOS_KERNEL_MINOR1=6 -DOS_KERNEL_MINOR2=18 -DWIZARD_ESCAN=0 -DBUILD_MODE_IMAGE -DXOC_FILE_ID=\"pg_templ:Makefile.local\" -DUNX -DGCC -DLINUX -DCPPU_ENV=gcc3 -DGXX_INCLUDE_PATH=/usr/include/c++/4.1.2 -DOS_LINUX -DNDEBUG -DHP93_SOC -DHP83_F330 -DWIZARD_NYI=1 -DX11R5 -Didnumber=domainname -DOS_LINUX -D_GNU_SOURCE -DHP_UX_10 -DJDK_13 -Wall -fno-strict-aliasing -DXOC_PROJECT_LOGGER="\"pg_templ\""
CFLAGS_DEBUG=-g  -I $(HP83000_ROOT)/pws/lib -I$(HP83000_ROOT)/prod_com/include -I$(HP83000_ROOT)/com/include -I$(HP83000_ROOT)/mix_sgnl/include -I $(V_UNO_DIR)/sdk/include/ -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -O -D_EXTENDED_ANSI -DHP83000_ROOT=\"/opt/hp93000/soc\" -DFORMATTER_ROOT=\"${SHIPMENT_ROOT}/segments/formatter\" -DHP83000_JAVA_ROOT=\"$(V_JAVA_DIR)\" -DOS_KERNEL=2 -DOS_KERNEL_MINOR1=6 -DOS_KERNEL_MINOR2=18 -DWIZARD_ESCAN=0 -DBUILD_MODE_IMAGE -DXOC_FILE_ID=\"pg_templ:Makefile.local\" -DUNX -DGCC -DLINUX -DCPPU_ENV=gcc3 -DGXX_INCLUDE_PATH=/usr/include/c++/4.1.2 -DOS_LINUX -DNDEBUG -DHP93_SOC -DHP83_F330 -DWIZARD_NYI=1 -DX11R5 -Didnumber=domainname -DOS_LINUX -D_GNU_SOURCE -DHP_UX_10 -DJDK_13 -Wall -fno-strict-aliasing -DXOC_PROJECT_LOGGER="\"pg_templ\""
CXXFLAGS=-I $(HP83000_ROOT)/pws/lib -I$(HP83000_ROOT)/prod_com/include -I$(HP83000_ROOT)/com/include -I$(HP83000_ROOT)/mix_sgnl/include -I $(V_UNO_DIR)/sdk/include/ -fmessage-length=0 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -O -D_EXTENDED_ANSI -DHP83000_ROOT=\"/opt/hp93000/soc\" -DFORMATTER_ROOT=\"${SHIPMENT_ROOT}/segments/formatter\" -DHP83000_JAVA_ROOT=\"$(V_JAVA_DIR)\" -DOS_KERNEL=2 -DOS_KERNEL_MINOR1=6 -DOS_KERNEL_MINOR2=18 -DWIZARD_ESCAN=0 -DBUILD_MODE_IMAGE -DXOC_FILE_ID=\"pg_templ:Makefile.local\" -DUNX -DGCC -DLINUX -DCPPU_ENV=gcc3 -DGXX_INCLUDE_PATH=/usr/include/c++/4.1.2 -DOS_LINUX -DNDEBUG -DHP93_SOC -DHP83_F330 -DWIZARD_NYI=1 -DX11R5 -Didnumber=domainname -DOS_LINUX -D_GNU_SOURCE -DHP_UX_10 -DJDK_13 -DStd=std -DUSING_NAMESPACE_STD=using\ namespace\ std\; -Wall -Wno-non-virtual-dtor -fno-strict-aliasing -DXOC_PROJECT_LOGGER="\"pg_templ\""
CXXFLAGS_DEBUG=-g -I $(HP83000_ROOT)/pws/lib -I$(HP83000_ROOT)/prod_com/include -I$(HP83000_ROOT)/com/include -I$(HP83000_ROOT)/mix_sgnl/include -I $(V_UNO_DIR)/sdk/include/ -fmessage-length=0 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -O -D_EXTENDED_ANSI -DHP83000_ROOT=\"/opt/hp93000/soc\" -DFORMATTER_ROOT=\"${SHIPMENT_ROOT}/segments/formatter\" -DHP83000_JAVA_ROOT=\"$(V_JAVA_DIR)\" -DOS_KERNEL=2 -DOS_KERNEL_MINOR1=6 -DOS_KERNEL_MINOR2=18 -DWIZARD_ESCAN=0 -DBUILD_MODE_IMAGE -DXOC_FILE_ID=\"pg_templ:Makefile.local\" -DUNX -DGCC -DLINUX -DCPPU_ENV=gcc3 -DGXX_INCLUDE_PATH=/usr/include/c++/4.1.2 -DOS_LINUX -DNDEBUG -DHP93_SOC -DHP83_F330 -DWIZARD_NYI=1 -DX11R5 -Didnumber=domainname -DOS_LINUX -D_GNU_SOURCE -DHP_UX_10 -DJDK_13 -DStd=std -DUSING_NAMESPACE_STD=using\ namespace\ std\; -Wall -Wno-non-virtual-dtor -fno-strict-aliasing -DXOC_PROJECT_LOGGER="\"pg_templ\""

# For makedepend to preserve the $(V_COMPILE_FLAG) flag 
# (which will be cut away at the make depend call)
# NOTE: Variable is set by Makefile.verigy.include at runtime
CFLAGS += $(XOC_COMPILE_FLAG)
CXXFLAGS += $(XOC_COMPILE_FLAG)

LD=$(CC)
LDFLAGS=-L$(HP83000_ROOT)/system/lib -Wl,-rpath,$(V_LOG4CXX_DIR)/lib -L$(V_LOG4CXX_DIR)/lib -llog4cxx -L$(V_PERFMON_DIR)/lib -L$(V_UNO_DIR)/ure/lib -Wl,-fatal-warnings -Wl,-melf$(V_MELF_SUFFIX)
POTENTIAL_GCOV_LDFLAGS_NEEDS_TOBE_AT_THE_END=

YACC=/usr/bin/bison
YFLAGS=-d

LEX=/usr/bin/flex
LFLAGS=

PATH=/bin:/usr/bin:/usr/atria/bin:/opt/aCC/bin:/opt/langtools/bin
SHELL=/bin/ksh

# Files to remove in make clean or make clobber
CLEAN_FILES=$(OBJS) core *~
PROGRAMS=$(DEST)/$(NAME)

CLOBBER_FILES=$(OBJS) $(PROGRAMS) makefile.Template

CI_BIN_SOURCE="$(HP83000_ROOT)/hp83000/prod_env/bin"
CI_TARGET =  .
CI = Ci

# Build an executable for each object
all:	$(PROGRAMS) $(COMMON_WINKIN_REQS_FILE)
	@rm -f $(CI_TARGET)/$(CI)
	@ln -s $(CI_BIN_SOURCE)/ci_program $(CI_TARGET)/$(CI)

$(PROGRAMS):    $(OBJS) $(EXTERNAL_OBJS) $(LIBS) $(BUILD_DIRS) $(COMMON_WINKIN_REQS_FILE)
		rm -f $@
		$(LD) -Wl,-z,now $(LDFLAGS) $(OBJS) $(EXTERNAL_OBJS) $(SH_LIBS) $(LIBS) $(SYSLIBS) $(POTENTIAL_GCOV_LDFLAGS_NEEDS_TOBE_AT_THE_END) \
		-o $(PROGRAMS)

debug:
	@$(MAKE) -f $(MAKEFILE) LDFLAGS="-g $(LDFLAGS)" CFLAGS="$(CFLAGS_DEBUG)" CXXFLAGS="$(CXXFLAGS_DEBUG)" $(OBJS)
	@rm -f $(CI_TARGET)/$(CI)
	@ln -s $(CI_BIN_SOURCE)/ci_program.d $(CI_TARGET)/$(CI)
	@$(MAKE) -f $(MAKEFILE) LDFLAGS="-g $(LDFLAGS)" CFLAGS="$(CFLAGS_DEBUG)" CXXFLAGS="$(CXXFLAGS_DEBUG)" $(PROGRAM)

clean:
	rm -f $(CLEAN_FILES)

clobber:
	rm -f $(CLEAN_FILES) $(CLOBBER_FILES)

# create 'makefile' out of 'Makefile' with added dependencies
# The command `echo $(CC) | cut -d ' ' -f1` below returns the first word
# of $(CC). $(CC) might contain addidional options (e.g. for purify support).
# makedepend needs to know, which compiler is used, because if a script like
# CC_hp83000 or CCP_hp83000 is used, makedepend needs to know, which options
# (include paths etc.) are set in the compiler script. makedepend then
# calls the compile script with the option -queryCompileFlags and expects the
# script to output the the options it uses for the compiler to stdout.
# Also add rules for building $(BUILD_DIRS) that create libraries and
# objects that are linked into the executable.
depend: $(COMMON_WINKIN_REQS_FILE)
	@$(HP83000_ROOT)/com/lbin/makedepend -f $(MAKEFILE) \
	                -c `echo $(CC) | cut -d ' ' -f1` \
	                -C `echo $(CXX) | cut -d ' ' -f1` \
                        -r $(PART_SRC_ROOT) \
	                -- -CFLAGS $(CFLAGS) \
	                   -CXXFLAGS $(CXXFLAGS) \
	                   -BUILD_DIRS $(BUILD_DIRS) \
	                -- \
	                $(SRCS)

# install nothing (target used for compatibility only)
install:
	@echo "nothing to be installed"

$(OBJS): $(GENERATED_HEADERS) $(COMMON_WINKIN_REQS_FILE)
MTMS =  
MTTS =  
