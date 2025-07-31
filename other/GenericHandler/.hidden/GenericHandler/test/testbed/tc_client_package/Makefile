CPP = /usr/bin/g++ -m64 -g
LD = /usr/bin/g++ -m64 -g

TCCT_PATH = $(XOC_SYSTEM)
  
INCLUDES =  -I $(TCCT_PATH)/include
LIB_PATHS = -L $(TCCT_PATH)/lib \
            -Wl,-rpath,:$(TCCT_PATH)/lib

CPPFLAGS = $(INCLUDES)
LDFLAGS = $(LIB_PATHS) -ltest_cell_api 


SRCS = $(shell ls *.cpp)
OBJS = $(SRCS:.cpp=.o)

all: tc_client_package 
clean: ; @rm -f $(OBJS); rm -f core*; rm -f tc_client_package 
tc_client_package: $(OBJS)
	${LD} $(OBJS) $(LDFLAGS) -o $@
.cpp.o:
	${CPP} $(CPPFLAGS)  -c $<
