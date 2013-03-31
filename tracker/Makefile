.SUFFIXES: .c .o

COMPILE = $(CC) -Wall -D_FILE_OFFSET_BITS=64 -g -DDEBUG_FLAG -DOS_LINUX
INC_PATH = -I../common -I/usr/local/include
LIB_PATH = -L/usr/local/lib  -ldl -rdynamic
TARGET_PATH = /usr/local/bin

SHARED_OBJS = ../common/hash.o ../common/chain.o ../common/base64.o \
              ../common/shared_func.o ../common/ini_file_reader.o \
              ../common/logger.o ../common/sockopt.o ../common/fdfs_global.o \
              ../common/sched_thread.o ../common/http_func.o \
              ../common/pthread_func.o ../common/md5.o \
              tracker_proto.o tracker_mem.o tracker_service.o \
              tracker_global.o tracker_func.o  tracker_dump.o ../common/linux_stack_trace.o

ALL_OBJS = $(SHARED_OBJS)

ALL_PRGS = fdfs_trackerd 

all: $(ALL_OBJS) $(ALL_PRGS)
.o:
	$(COMPILE) -o $@ $<  $(SHARED_OBJS) $(LIB_PATH) $(INC_PATH)
.c:
	$(COMPILE) -o $@ $<  $(ALL_OBJS) $(LIB_PATH) $(INC_PATH)
.c.o:
	$(COMPILE) -c -o $@ $<  $(INC_PATH)
install:
	mkdir -p $(TARGET_PATH)
	cp -f $(ALL_PRGS) $(TARGET_PATH)
clean:
	rm -f $(ALL_OBJS) $(ALL_PRGS)
