#*******************************************************************************
#  File Name  : Makefile  
#  Author     : weiming   
#  Date       : 2012/06/14  
#  cmd        : make  
#*******************************************************************************  
  
SER_NAME = CMD
#M_DATE = `date '+%y%m%d'`
M_DATE = Application


CROSS_COMPILE = arm-fsl-linux-gnueabi-
CXX = $(CROSS_COMPILE)gcc
#AR  = ar cr
COMPILE_FLAGS = -Wall

ifdef CROSS_COMPILE
THIRDLIBS = arm-lib
else
THIRDLIBS = x86-lib
endif
  
INCLUDE_PATH = -I$(PWD)/$(THIRDLIBS)/include

LIB_PATH = -L$(PWD)/$(THIRDLIBS)/lib

LIBS = -lpthread -liniparser -lsocketcan -ljpeg -lv4l2 -lv4lconvert -liconv

SRC = $(wildcard *.c)

OBJS = $(SRC:.c=.o)

all: $(OBJS) $(SER_NAME)

.PHONY:all clean install

$(SER_NAME):$(OBJS)
	$(CXX) -o $(SER_NAME)_$(M_DATE) $(OBJS) $(INCLUDE_PATH) $(LIB_PATH) $(LIBS)

%.o : %.c
	$(CXX) -c $(COMPILE_FLAGS) $(INCLUDE_PATH) $< -o $@

install:
	cp -rPf $(SER_NAME)_$(M_DATE) /home/bqin/work/imx/nfsroot

clean:
	-rm *.o $(SER_NAME)_$(M_DATE)
