#*******************************************************************************
#  File Name  : Makefile  
#  Author     : weiming   
#  Date       : 2012/06/14  
#  cmd        : make  
#*******************************************************************************  
  
SER_NAME = cma
#M_DATE = `date '+%y%m%d'`
M_DATE = app


#CROSS_COMPILE = /opt/freescale/usr/local/gcc-4.6.2-glibc-2.13-linaro-multilib-2011.12/fsl-linaro-toolchain/bin/arm-linux-
CXX = $(CROSS_COMPILE)gcc
#AR  = ar cr
COMPILE_FLAGS = -Wall

ifdef CROSS_COMPILE
THIRDLIBS = arm-lib
else
THIRDLIBS = x86-lib
endif
  
INCLUDE_PATH = -I/home/qinbh/Dropbox/src/CMD_SourceCode/$(THIRDLIBS)/include

LIB_PATH = -L/home/qinbh/Dropbox/src/CMD_SourceCode/$(THIRDLIBS)/lib

LIBS = -lpthread -liniparser -lsocketcan -ljpeg -lv4l2 -lv4lconvert

SRC = $(wildcard *.c)

OBJS = $(SRC:.c=.o)

all: $(OBJS) $(SER_NAME)

.PHONY:all clean

$(SER_NAME):$(OBJS)
	$(CXX) -o $(SER_NAME)_$(M_DATE) $(OBJS) $(INCLUDE_PATH) $(LIB_PATH) $(LIBS)

%.o : %.c
	$(CXX) -c $(COMPILE_FLAGS) $(INCLUDE_PATH) $< -o $@

clean:
	-rm *.o $(SER_NAME)_$(M_DATE)
